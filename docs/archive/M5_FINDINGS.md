# M5 — Multi-sessione locale: cosa è stato costruito e cosa si è imparato

Milestone M5 di `DESIGN.md` §8: *due `Session` nello stesso processo (due mondi
locali), una attiva e una passiva renderizzata nel portale, swap in place con
tutte le invarianti di §4.3.*
**Test:** *entra e esce dieci volte di fila; cielo, tasti premuti, avatar proprio
e view range corretti a ogni giro (i quattro bug storici).*

**In corso.** Sotto c'è solo ciò che è stato costruito e verificato da
esecuzioni.

---

## 1. La partenza era migliore del previsto

`DESIGN.md` §4.1 dice che tutta la difficoltà del dual-client nel fork viene
dallo stato globale, e che le invarianti sono «tutte, cicatrici da singleton».
La prima cosa da misurare era quindi quanto stato globale ci fosse qui.

Quasi nulla: tre variabili di file in `main.cpp` (il target di cattura dei gate,
l'estensione del mondo, il flag degli screenshot) e una nella piattaforma
(`g_cursor_grabbed`). Nessuna in `core/`.

`Scene` era già, di fatto, una sessione più i renderer. L'estrazione di
`Session` ha spostato dodici campi — registry, mondo, mesh, mod, le due aperture
in entrambe le forme, i trigger — e ha lasciato in `Scene` ciò che è per
processo: gli shader dei portali, della selezione e dei corpi, e lo stato
dell'unico giocatore (roll, warp della camera).

**525 usi riscritti, zero cambiamenti di comportamento, 22 test su 22 verdi.**

## 2. Due dei quattro bug storici non erano nemmeno rappresentabili

Il cielo era la costante `kSkyColour`, letta in tre punti. Il view range non
esisteva: `WorldRenderer::draw` faceva frustum culling e basta. Non si poteva
sbagliare, e non si poteva provare.

- **Cielo per sessione.** `Session::sky`, e `PortalRenderer::set_sky` per il
  pass che disegna l'altro mondo. Il portale ora si pulisce col cielo del mondo
  che mostra, non con quello del processo.
- **View range per sessione.** `WorldRenderer::set_view_range`, con il culling
  per distanza misurato al punto **più vicino** del chunk e non al centro: un
  test sul centro fa sparire il chunk su cui il giocatore è in piedi appena il
  raggio scende sotto mezzo chunk, cioè un buco nel pavimento invece che uno
  lontano. Attiva 512, passiva 512/3 come da §4.2.

Il default del raggio è enorme, quindi ogni gate scritto prima disegna
esattamente ciò che disegnava.

## 3. Il portale che guarda in un altro mondo

Due righe, una volta che cielo e raggio hanno smesso di essere costanti:

```c
Session &shown = s.act().links_to_other ? s.other() : s.act();
shown.world_render.set_view_range(shown.view_range);
s.portal_render.set_sky(shown.sky);
```

`links_to_other` è **spento** di default, e non per pigrizia: ogni gate di
questo motore collega un mondo a sé stesso, e un flag col default opposto
riscriverebbe in un colpo solo ciò che tutti misurano.

Misurato con la disciplina delle altre continuità — punto di vista ricavato a
mano, e poi un confronto contro una posa **sbagliata in modo dichiarato**:

| L'apertura di A, vista da 3 nodi | |
|---|---|
| contro il mondo **B** dal punto ricavato a mano | **0.00%** su 61424 pixel |
| contro il mondo **A** dallo stesso punto | **29.56%** |

Il secondo numero è quello che dice che il primo sta leggendo qualcosa: è
esattamente ciò che si vedrebbe se lo swap del mondo fosse dimenticato.

## 4. I due mondi devono essere due mondi

Il primo tentativo campionava il terreno dei due mondi fra x,z 20 e 100 e
riportava **2 colonne diverse su 144**. Non era un difetto del generatore: la
sua *piazza* è piatta a `kGroundTop` esattamente su `[20,100]`, quindi lì due
seed danno terreno identico per costruzione. Il check misurava la sola regione
in cui la risposta è "uguali".

Campionando fuori dalla piazza e dai suoi 16 nodi di raccordo: **90 colonne
diverse su 108**.

E per non dipendere solo dalle colline lontane, ogni mondo ha un suo landmark
davanti alla propria apertura — *davanti*, perché la camera virtuale sta dietro
l'apertura e guarda fuori: la prima versione lo metteva dietro e la misura non
cambiava di nulla (27.36% prima, 27.36% dopo). Spostato davanti: 29.56%.

## 5. Lo swap, e i quattro bug

```c
s.act().draws_own_avatar = false;          // demozione, PRIMA che l'indice si muova
s.act().view_range       = kPassiveViewRange;
s.active = 1 - s.active;
s.act().draws_own_avatar = true;           // promozione
s.act().view_range       = kActiveViewRange;
```

Il cielo non è toccato: è già quello della sessione, tenuto **vivo** nella
struct invece che riprodotto dagli eventi — l'invariante che il fork ha pagato,
perché un server manda `set_sky` una volta sola e al secondo giro il replay è
vuoto.

Dieci giri, `10 laps: sky wrong 0, avatar wrong 0, range wrong 0, keys dead 0,
buried 0`.

| Sabotaggio | Esito |
|---|---|
| il cielo viene portato dal mondo che si lascia | **rosso**, 5 giri su 10 |
| nessuna demozione dell'avatar | **rosso**, 10 su 10 |
| nessuna demozione del view range | **rosso**, 10 su 10 |
| l'arrivo è validato contro il mondo di partenza | **rosso**, vedi §6 |

Un sabotaggio provato e **scartato**: `draws_own_avatar` invertito invece che
assegnato resta verde, perché la demozione esplicita garantisce che la sessione
promossa sia già `false`. Il toggle non è un difetto qui, ed è più onesto dirlo
che spacciarlo per sabotaggio.

## 6. L'arrivo si giudica col mondo in cui si atterra

`portal_traverse` chiede se i piedi ci stanno, e i blocchi da interrogare sono
quelli della **destinazione**. Passargli il mondo di partenza è un errore di una
parola che nessuno degli altri check nota: entrambi i mondi sono piazza piatta
all'uscita, quindi le due risposte coincidono.

Due tentativi di renderlo osservabile con un ostacolo sono falliti, e tutti e
due hanno insegnato qualcosa:

- **Un gradino davanti all'apertura** rende la camminata impossibile. Un nodo è
  un muro: `move_box` non ha auto-step, quindi chi torna o si ferma contro la
  sua faccia o ci sale sopra e sbatte la testa nel solaio sopra l'apertura. Il
  gate riportava 1 giro su 10.
- **Un blocco sotto i piedi d'arrivo** non può esistere. Da quando l'uscita
  atterra dove la mappa la manda (§16 di M4), i piedi arrivano **dentro il nodo
  della destinazione**, che è il buco. Qualunque cosa si metta lì è il buco
  tappato.

Quindi il caso tappa il buco **apposta**, e l'invariante che controlla è quella
che ne segue: una destinazione murata **rifiuta** l'attraversamento invece di
inghiottire chi ci prova — e può rifiutare solo se le si è chiesto del mondo
giusto.

```
walled destination: crossed 0, still in world 0        (corretto)
walled destination: crossed 1, still in world 1        (sabotato)
```

## 7. Cosa resta fuori, dichiarato

- **I tasti premuti non sono stato di sessione in questo client.** L'input è
  ricostruito ogni frame da `plat_key_down`, quindi il bug storico del fork —
  lo swap che azzera i keydown invece dei soli edge — **non è rappresentabile**
  qui: è prevenuto per costruzione, non risolto. Il check che porta quel nome
  misura la cosa vicina che *può* rompersi, cioè che dopo lo swap il giocatore
  si muova nel mondo in cui è arrivato. Detto invece di lasciarlo sembrare una
  vittoria.
- **Il budget della passiva è il view range e il tick di mesh.** §4.2 chiede
  anche niente audio, niente particelle, niente predizione e RT alla
  risoluzione del portale: quelli non ci sono. Il tick di mesh differenziato
  per sessione è arrivato con §8 — un chunk per frame contro quattro — e con
  esso l'obbligo di dire quanto costa: al ritmo passivo un mondo intero impiega
  qualche secondo a comparire, per cui la scena `--dual` ne fa il mesh completo
  una volta sola all'avvio e lascia al tick solo le modifiche successive.
- ~~**Nessuna misura di costo.**~~ Misurata: vedi §10. Restano fuori gli altri
  item del budget passivo — niente audio e niente particelle (non esistono
  ancora), niente soppressione della predizione, e **RT alla risoluzione dello
  schermo invece che del portale**, che §10 indica come la prossima leva.
- **Una sola apertura per mondo** nella modalità a due sessioni, e due sessioni
  al massimo (`kMaxSessions`). La ricorsione dentro un portale che guarda un
  altro mondo usa un solo cielo per tutti i livelli, il che è giusto per due
  mondi e sbagliato per tre.
- **Niente rete.** M5 è multi-sessione *locale*; ticket, identità e scope sono
  M6 (§4.4).

---

## 8. Due difetti che solo un giocatore poteva vedere

La scena `--dual` è stata percorsa a piedi. I gate erano tutti verdi. Chi ci ha
camminato ha riportato due cose, ed entrambe erano difetti veri.

### 8.1 «Prima del primo passaggio vedo solo il cielo»

Il ciclo di frame faceva il mesh di una sola sessione:

```cpp
s.act().world_render.sync(s.act().world, s.act().registry);
```

L'altro mondo — quello dentro l'apertura — non aveva **nessuna geometria
costruita**. Il render target si puliva col suo cielo e non ci disegnava niente
sopra. Dopo il primo attraversamento quel mondo diventava attivo, veniva
meshato, e da lì in poi la scena sembrava funzionare: è per questo che il
sintomo spariva da solo e sembrava un problema di caricamento.

Misura, con la sola apertura sullo schermo come regione: **100.00%
dell'apertura era cielo nudo di B**. Ora è **6.67%**.

Perché il gate non lo vedeva: `run_dual` si costruisce le proprie sessioni e le
fa *settle* entrambe prima di misurare qualsiasi cosa. Un gate che si costruisce
la scena misura la scena che si è costruito. La sezione 0 del gate ora misura
prima del settle, cioè lo stato in cui un giocatore nasce, e il sabotaggio che
toglie il tick passivo la fa diventare rossa con quel `100.00%` stampato.

Il tick passivo è **1 chunk per frame** contro i 4 dell'attiva. Non è un numero
misurato: è una frazione, e quello che conta è che il mondo passivo sia più
lento di quello in cui si cammina, non che sia esattamente questo ritmo. Il
check lo dice in quei termini — `passive_first > 0 && passive_first <
active_first` — e le due metà stanno in un check solo perché «minore
dell'attiva» è vero anche di zero: con il tick passivo rimosso quel confronto
leggeva `0 < 4` e restava verde mentre il check sopra diventava rosso.

### 8.2 Il lampo del mondo di partenza

Il secondo sintomo, testuale: «quando passo verso l'altro mondo, per un breve
istante, vedo il mondo da cui sto partendo», con l'ipotesi «nel punto di
teleport il portale scompare troppo presto». L'ipotesi era giusta, col segno
invertito: il teletrasporto scattava troppo presto.

Per misurarlo il gate ora **filma** i frame intorno all'attraversamento. Due
numeri per frame: quanto schermo sta dipingendo l'apertura, ottenuto rendendo
lo stesso frame due volte, con e senza le aperture, e contando cosa cambia; e
quanta parte dello schermo è il colore di cielo di ciascuno dei due mondi. I
due cieli erano stati scelti diversi perché «quale mondo sto guardando» fosse
una domanda a cui può rispondere un pixel; qui serve per la seconda volta.

Prima:

```
f11  world A  depth +0.317  opening 87.03%  A sky  0.00%  B sky  9.93%
f12  world B  depth -0.237  opening 88.57%  A sky 16.25%  B sky  0.00%   <- crossed
f13  world B  depth -0.157  opening 90.81%  A sky 20.74%  B sky  0.00%
f14  world B  depth -0.077  opening 94.10%  A sky 27.51%  B sky  0.00%
f15  world B  depth +0.003  opening 19.90%  A sky  0.00%  B sky 25.37%
```

Tre frame con `depth` **negativa**: il giocatore era dietro al piano
dell'apertura di destinazione, quell'apertura gli copriva il 94% dello schermo,
e ci dipingeva dentro il mondo A.

La causa è una riga sola. `past_trigger` scattava a `depth < 0.5 -
kTriggerDepth`, cioè **un quarto di nodo davanti** al piano di partenza. La
mappa nega la componente normale, quindi chi è ancora davanti alla sorgente
esce ancora **corto** rispetto alla destinazione, dalla parte sbagliata della
porta. I tre frame sono il tempo che ci vuole a camminare quel quarto di nodo.

Il trigger ora scatta **al piano** (`kTriggerDepth = 0.0f`). L'occhio è oltre
di quanto lo ha portato un frame di movimento, e la mappa gira quel segno:
l'uscita è davanti alla destinazione della stessa quantità, con la porta alle
spalle. Zero non è un valore tarato — è l'unica profondità a cui i due lati
dell'attraversamento sono d'accordo su da che parte del muro sta il giocatore.

Dopo:

```
f14  world A  depth +0.077  opening 94.10%  A sky  0.00%  B sky 20.68%
f15  world B  depth +0.020  opening 17.19%  A sky  0.00%  B sky 26.50%   <- crossed
f16  world B  depth +0.100  opening  0.62%  A sky  0.00%  B sky 32.82%
```

Zero frame col cielo di A dopo lo swap, e la frazione di cielo di B cresce
liscia attraverso lo swap (20.68 → 26.50 → 32.82) invece di saltare.

### 8.3 La correzione che è stata scartata

Il primo tentativo era doppio: oltre al trigger, non dipingere l'immagine
dell'apertura quando la telecamera sta **dietro** al piano. Sembrava
principiato — la telecamera virtuale è quella reale specchiata, quindi da
dietro la sorgente finisce *davanti* alla destinazione e il piano obliquo
taglia via ciò che l'apertura dovrebbe mostrare.

Ha fatto diventare rosso `portal_cut`:

```
FAIL  and the copy lands exactly where the removed half was: cutting is invisible
```

Quel gate fotografa un'apertura **da dietro** apposta, ed è l'unico punto di
vista da cui il taglio di un corpo a metà è visibile. Misurava byte per byte
che la copia atterra esattamente dove stava la metà rimossa. Un'immagine che
regge una misura del genere non è spazzatura: l'apertura è una **porta a due
facce**, e da dietro mostra il retro del mondo di destinazione, in modo
simmetrico e coerente.

Quindi la seconda metà è stata **tolta**, non aggiustata. Il trigger da solo
porta il lampo a zero, e togliere una funzione che regge una misura per
nascondere un caso che il trigger non produce più sarebbe stato pagare due
volte. Il commento su `kTriggerDepth` lo dice, perché il prossimo che vede
un'apertura dipinta da dietro non ripercorra la stessa strada.

### 8.4 Una asimmetria larga 1,62 nodi, trovata di rimbalzo

Spostare il trigger ha fatto diventare rosso un check che non c'entrava:

```
FAIL  standing still under it does not
```

`portal_step_trigger` decideva «è entrato dal davanti?» guardando l'**occhio**,
ma per un portale orizzontale fa scattare l'attraversamento sui **piedi** — che
stanno 1,62 nodi più in basso. Con la vecchia soglia i due errori si
mascheravano a vicenda. Con la nuova, uno fermo sotto un portale a soffitto con
la testa che sporge dentro contava come «entrato dal davanti» e veniva tirato su
attraverso un portale sotto cui stava semplicemente camminando.

Ora entrambe le domande usano lo stesso punto:

```cpp
const float depth = portal_depth(p, p.horizontal() ? feet : eye);
```

Non è una correzione trovata leggendo il codice: è caduta fuori dal fatto che
cambiare una soglia ha smesso di mascherarla.

### 8.5 Cosa dice questo sui gate

`--dual-check` aveva 15 check verdi e nessuno dei due difetti era visibile. Il
primo perché il gate si settla la scena che il giocatore non ha; il secondo
perché ogni misura era presa **a riposo** — la posa dopo l'attraversamento, i
flag dopo, l'immagine da un punto di vista derivato a mano — e nessuna guardava
i **frame** dell'attraversamento.

Il rimedio è nel gate, non nel verbale: la sezione 0 misura prima del settle, e
la sezione 4 filma dieci frame intorno allo swap. Il costo è un secondo di
esecuzione; il difetto costava a chiunque ci camminasse dentro.

### 8.6 Lo spawn dentro una colonna

Terza segnalazione dalla stessa camminata: lo spawn coincideva con una colonna.

`stage_scatter` sparge colonne di pietra alte da 2 a 5 su tutta la mappa, tre
possibilità su 256 per colonna. Il punto di spawn è **derivato** dal portale —
6,5 nodi davanti all'apertura, sul terreno — e la derivazione non può sapere
che lo scatter ha messo una colonna proprio lì. Sul seed 1, quello che `--dual`
esegue, ce l'aveva messa.

La correzione non è spostare la costante. Sei nodi e mezzo invece di sei e
mezzo aggiusta un seed e lascia il prossimo da scoprire allo stesso modo:
camminandoci dentro. `spawn_in_front` tiene la derivazione e cerca di lato,
mezzo nodo per volta e alternando, il primo punto in cui `inside_solid` — lo
stesso predicato che usa il server — dice che un corpo ci sta. Di lato perché
è la direzione che conserva sia la distanza dall'apertura sia la linea di
vista, cioè le due cose per cui il punto era derivato.

Mondo A: `40.5 → 41.5`. Mondo B: non si muove, era già libero. Il check lo
misura su entrambi i mondi, perché o l'uno o l'altro può essere quello in cui
si comincia, e il sabotaggio che toglie la ricerca lo fa diventare rosso
esattamente sul caso segnalato.

---

## 9. La forma del portale, in un file

Tre numeri chiesti a voce — le facce laterali dall'inizio del blocco, il
teletrasporto al 10% del blocco, l'uscita al 10% del blocco d'uscita — e la
richiesta che fossero configurabili. Il risultato è `config/portal.cfg` e
`core/portal/portal_tuning.{h,cpp}`.

### 9.1 La convenzione

Un portale è un **blocco**, spesso un nodo. Ogni numero è una frazione di quel
blocco misurata **lungo la direzione di marcia dalla faccia da cui il giocatore
ci entra**: 0.0 è la faccia da cui entra, 1.0 quella da cui esce.

```
plane        = 1.00   # dove sta l'immagine dentro il blocco
tunnel_start = 0.00   # dove cominciano le facce laterali
trigger      = 0.10   # a che profondità scatta l'attraversamento
exit         = 0.90   # dove viene messo il giocatore nel blocco d'uscita
```

Le due conversioni in profondità **non sono la stessa formula**, e non è una
svista: nel blocco di partenza si entra dalla faccia anteriore, a +0.5 dal
centro del nodo, e si va verso l'interno; in quello d'arrivo si entra dalla
faccia posteriore, a −0.5, e si va verso l'esterno. Stessa frase, estremi
opposti.

### 9.2 Il piano si è spostato, e con lui otto gate

`plane = 1.00` mette l'immagine sulla faccia opposta del blocco. Non è solo un
quad che si sposta: quel piano è dove `portal_clip` taglia un corpo, dove
`portal_math` incolla i due mondi, dove il piano obliquo di Lengyel è ancorato,
e dove il tunnel finisce. Sono cinque file che devono essere d'accordo, e il
modo di tenerli tali è che nessuno di loro scriva `center`: `center` resta il
**centro del nodo** — ogni scena, ogni mod e ogni gate piazzano i portali per
nodo — e il piano si chiede a `portal_plane_point()`.

Spostarlo ha fatto diventare rossi **nove test su ventitré**. Nessuno per un
difetto: ognuno aveva la propria vista di riferimento derivata a mano con il
piano al centro del nodo. La correzione è un termine solo, e vale per tutti:

> lo specchio è **fra i piani**, quindi spostare il piano sorgente di `p` lungo
> la sua normale e quello di destinazione di `p` lungo la propria sposta la
> risposta di **2p lungo la normale di destinazione**.

Resta una derivazione a mano: usa un'impostazione e un mezzo giro, non la
funzione sotto esame. Una vista calcolata con `portal_virtual_camera` si
sposterebbe insieme a ciò che dovrebbe controllare.

### 9.3 Due viste che il vano di porta ha reso cieche

Due gate non si sono aggiustati con quel termine, e la ragione è la stessa in
entrambi: **un vano profondo un nodo non si vede attraverso a quarantacinque
gradi.**

- `portal_cut` guarda l'apertura da 2,2 nodi in fuori e 2,2 di lato per vedere
  la striscia di apertura accanto alla cassa. Con il tunnel profondo mezzo nodo
  quella striscia c'era; con un nodo intero non c'è: **0 pixel** di rivelazione
  su 8234. Spostata la vista, 3147 su 42915.
- `portal_orientation`, casi 2 e 3, guardava un portale a pavimento da due nodi
  sopra e due di lato. Dentro l'apertura, **2730 pixel su 3982 erano il colore
  del tunnel** — esattamente `kPortalWallColour`, che è come è stato
  diagnosticato. Da tre sopra e uno di lato (diciotto gradi) lo scarto laterale
  su quella profondità è un terzo di nodo contro un'apertura lunga due, e la
  regione confrontabile passa da 3982 a **48140 pixel**, con 0 differenze.

Non è una soglia allargata: è la vista che deve stare all'altezza della
geometria. Il numero che lo dimostra è quello contro la posa sbagliata, che
resta alto — 88,55%, 78,25%, 72,77%.

### 9.4 Il prezzo, misurato e non nascosto

`exit` è una **posizione**, non uno scarto. L'immagine che il giocatore sta
guardando è disegnata da una mappa che incolla i due piani, quindi l'unica
uscita che la continua è la risposta della mappa stessa:

```
exit_senza_cuciture = 1.0 - trigger + 2*(1.0 - plane)
```

Con `plane = 0.50` e `trigger = 0.10` viene `exit = 0.10`, raggiungibile. Con
`plane = 1.00` viene **negativo** — fuori dal blocco — il che dice una cosa
precisa: *un vano profondo un nodo intero e un'uscita dentro il blocco d'uscita
non possono essere veri e senza cuciture allo stesso tempo.*

Le impostazioni chieste sono la seconda delle due. Il salto è quindi reale, e i
gate lo **misurano** invece di darlo per buono o per cattivo:

```
the eye landed 1.8000 from where the opening was showing it (the settings predict 1.8000)
NOTE: this crossing lurches 1.80 nodes. exit = -0.90 is the value that makes it
      zero, which is outside the block: no exit is seamless at this plane.
```

e, in pixel, dal gate che confronta il fotogramma che stavi guardando con quello
in cui arrivi:

```
the arrival frame differs from the frame being shown in 723486 of 2073600 pixels (34.89%)
```

**Un terzo dello schermo cambia nell'istante dell'attraversamento.** Il check
non è stato cancellato né allargato: adesso pretende che lo scarto sia
*esattamente quello che le impostazioni predicono*, al millesimo, e quando le
impostazioni predicono zero torna a pretendere zero. Così continua a prendere le
derive senza fingere che un salto configurato sia un bug — e il costo resta
stampato a ogni esecuzione.

### 9.5 Cosa NON è configurabile, detto

- Il file si legge **all'avvio**, una volta. Niente ricarica a caldo.
- I valori fuori intervallo, o `tunnel_start`/`trigger` oltre `plane`, sono un
  **errore fatale**: il client esce. Una forma metà applicata è peggio di quella
  vecchia.
- Una chiave sconosciuta è un errore, non un'alzata di spalle: un refuso che
  silenziosamente non fa niente costa un pomeriggio.
- Il test unitario `portal_traverse` carica lo **stesso** file del client. Un
  test che misura una forma mentre il gioco ne usa un'altra è un test d'accordo
  con sé stesso.

### 9.6 Il muro dell'altro mondo, e i due piani

Provata la scena: «ho visto il salto ma bisogna spostare il punto di vista della
telecamera che non deve mostrare il muro dell'altro mondo».

Aveva ragione, ed era un errore mio nella prima versione. Il piano veniva preso
allo stesso modo alle due estremità — `center + normal * plane_depth` — cioè
**faccia posteriore di A incollata a faccia posteriore di B**. Il piano di taglio
obliquo cadeva quindi sulla faccia posteriore di B, e nell'immagine restava tutto
lo spessore del muro di B: un secondo vano di porta dietro il primo.

Un vano di porta si attraversa **una volta**. La faccia da cui esci dalla
sorgente va incollata alla faccia da cui **emergi** alla destinazione, che è
l'immagine speculare:

```cpp
portal_plane_in (s) = center + normal * plane_depth();   // dove entri
portal_plane_out(s) = center - normal * plane_depth();   // da dove esci
```

`portal_transform` usa `in` per la sorgente e `out` per la destinazione; il
piano obliquo e i cinque piani di taglio usano `out`; il quad dell'immagine e il
tunnel usano `in`; `portal_cut` taglia sul piano `in` della sorgente e tiene la
copia contro il piano `out` della destinazione.

Tre conseguenze, tutte misurate:

1. **Il muro sparisce.** Il piano di taglio è ora la faccia da cui si emerge, e
   il blocco della destinazione è interamente davanti a esso: nell'immagine non
   entra.
2. **I due scarti si annullano nella mappa.** La telecamera virtuale torna
   esattamente dov'era quando l'immagine stava al centro del nodo, e tutte le
   viste di riferimento derivate a mano nei gate **riprendono il loro valore
   originale**: i termini `2p` che avevo aggiunto in §9.2 sono stati tolti tutti.
   Un buon segnale: la correzione semplifica invece di aggiungere.
3. **Il salto scende da 1,80 a 0,80 nodi**, e l'uscita senza cuciture diventa
   raggiungibile e semplice da dire: `exit = trigger` — atterrare tanto dentro
   al blocco d'uscita quanto si era entrati in quello d'ingresso. Sullo schermo
   il costo passa dal 34,89% al **29,24%** dei pixel.

Il difetto è stato preso da `portal_clip` prima ancora che dai pixel: usando il
piano d'ingresso a entrambi i capi, le due metà di un corpo si sovrappongono del
doppio dello scarto, e il gate ha stampato **36 coppie di orientamento su 36
tagliate male**.

### 9.7 Le facce laterali come finestra

Domanda dalla prova: le facce laterali mostrano la destinazione come il fondo?
No — erano dipinte a tinta unita, `kPortalWallColour`, e solo il quad sul piano
campionava il render target. Scelta l'opzione di farne una finestra, con la
motivazione: **nella destinazione potrebbe non esserci un muro.**

Il motivo per cui erano piatte era vero e documentato: il target non ha niente
da mostrargli. È ritagliato da quattro piani laterali che formano una piramide
dalla telecamera virtuale attraverso l'apertura, e quei frammenti cadono fuori —
campionarlo lì aveva riempito di colore di sfondo il fondo di un'apertura a
livello del pavimento.

**Il primo tentativo è stato allargare la piramide**, ancorando i quattro piani
all'apertura della *bocca* del vano invece che a quella del piano immagine.
Sbagliato, e il gate lo ha detto subito: `kApexStandoff` tira l'apice della
piramide a 0,05 dalla faccia, quindi una bocca un nodo più indietro cade
**dietro** l'apice, ogni piano si rovescia e l'immagine perde metà dei pixel —
**49,38% dell'apertura diverso dal riferimento a 6,5 nodi**, contro una soglia
del 2%. Avevo scritto, prima di misurare, che «quello che l'apertura mostra non
cambia». Falso, e i gate lo hanno preso.

**Il meccanismo giusto è togliere quei quattro piani**, non allargarli. Esistono
per nascondere ciò che sta *accanto* all'apertura della destinazione, che è
esattamente l'unica cosa che una finestra deve mostrare.

Toglierli è sicuro solo grazie a §9.6. Il piano di profondità sta ora sulla
faccia da cui il giocatore emerge, quindi il blocco della destinazione è
interamente dietro di esso e non può filtrare nell'apertura da nessun angolo.
Non era vero finché l'immagine stava al centro del nodo, dove metà di quel
blocco stava davanti al piano di taglio — ed è per quello che i quattro piani
erano stati scritti.

Misure dopo: continuità **0,00% / 0,00% / 1,15%** alle tre distanze, invariata.
23/23.

Configurabile, perché le due forme sono entrambe difendibili:

```
sides = 1   # finestra: mostra cosa c'è accanto all'apertura di destinazione
sides = 0   # muro: tinta unita, onesta sull'essere il muro di questo mondo
```

Il test unitario di `portal_math` misura i quattro piani laterali chiedendo
esplicitamente la forma `sides = 0` — sono la cosa sotto esame — e aggiunge un
check che con `sides = 1` ne resti **uno solo**, e che il punto che quei piani
toglievano adesso arrivi all'immagine.

### 9.8 Il cielo sui bordi: il piano vicino stava un blocco troppo avanti

Dalla prova: «il fatto che si veda il cielo sui bordi del portale probabilmente
è dovuto all'oblique near clipping plane piazzato un blocco troppo avanti».
Diagnosi giusta, e il numero l'ha confermata prima ancora dei pixel.

Dopo §9.6 il piano di taglio stava sulla **faccia da cui il giocatore emerge**,
cioè la faccia anteriore del blocco di destinazione. Quel blocco era quindi
interamente tagliato via. Fino a §9.7 non si notava: le facce laterali erano
tinta unita e non chiedevano niente al target. Diventate finestre, chiedono
proprio quello — cosa c'è accanto all'apertura della destinazione — e la
risposta era: niente, colore di sfondo, cioè cielo.

Un vano di porta è **un tunnel con due nomi**: le facce laterali che il
giocatore sta guardando sono quelle della sorgente, e lo stesso volume alla
destinazione è il suo blocco. Il piano di taglio va all'**estremità lontana del
vano**, un `doorway_depth` dietro la faccia d'uscita:

```cpp
portal_clip_point(s) = portal_plane_out(s) - s.normal * doorway_depth();
```

Così quel blocco viene disegnato, e cade **nello stesso volume** del vano invece
che dietro di esso — un tunnel solo, visto come quello della destinazione. Ciò
che non deve mai succedere è che finisca *oltre* l'immagine, che è quello che
faceva incollare i due piani d'ingresso fra loro: quello sì si legge come un
secondo vano (§9.6).

La misura, presa dal gate di continuità senza cercarla: a 1,5 nodi il confronto
passa da **1,15% a 0,00%** di pixel diversi dal riferimento. Quell'1,15% non era
tolleranza di rasterizzazione come sembrava — era il nodo mancante. Per essere
esatti su quando è comparso: non c'era prima di questa sessione. Con
`plane = 0.5` il gate legge 0,00%, e quella è la forma che il motore aveva
finora; l'1,15% è arrivato spostando il piano e se n'è andato rimettendo a posto
il piano di taglio.

**E qui sta la lezione che vale più della correzione.** Quel valore è stato
stampato `PASS` a ogni esecuzione, perché la soglia era il 2% e il commento
accanto diceva: «anything structural -- a seam, a wrong projection, geometry
leaking through -- moves this by whole percentage points, not tenths». Era
falso, e a smentirlo è bastato tagliare via un nodo intero dell'immagine: 1,15%,
strutturale per qualsiasi definizione, sotto soglia.

La soglia è ora **0,10%**. Non è un numero scelto a occhio: entrambe le forme
che `config/portal.cfg` può assumere leggono **esattamente 0** pixel diversi su
12960, 60726 e 242900, ripetibile fra esecuzioni. Lo 0,10% lascia comunque 243
pixel di margine alla distanza più corta — più di quanto possa consumare un
bordo condiviso — ed è undici volte più stretto del difetto che ci si è
nascosto dentro. Se un giorno diventa rossa su un cambiamento giusto, la
risposta è guardare quali pixel si sono mossi, non rimettere il 2.

Con l'immagine al centro del blocco `doorway_depth` è zero e il piano torna
esattamente dov'è sempre stato.

Il test di `portal_math` teneva una sola profondità e ora ne tiene due, perché
erano lo stesso punto e non lo sono più: `az`, l'apertura attraverso cui sono
tracciati i quattro piani laterali, e `cz`, il piano di profondità un vano più
indietro. Confonderle è la ragione per cui quella sezione andava riscritta.

### 9.9 Il punto di vista un blocco più avanti

Richiesta dal gioco, dopo il piano di taglio: «sposta ancora la telecamera di
uscita di 1 blocco in modo da non vedere il muro e vedere direttamente l'area
appena fuori dal portale».

La parete che si vedeva è il vano di porta della destinazione visto da dentro.
Con `plane = 1.00` la mappa incolla la faccia da cui si esce alla faccia da cui
si entra, quindi un giocatore fermo a un blocco davanti all'apertura ha il
punto di vista **dentro** il blocco d'arrivo: l'immagine è corretta — è
esattamente ciò che mostrerebbe un buco vero nel muro — e mostra il tunnel,
perché il tunnel è ciò che c'è lì.

Nuovo numero in `config/portal.cfg`:

```
view = 1.00
```

Quanto più in là della faccia d'uscita si guarda, in blocchi. È l'unico numero
del file che riguarda ciò che l'apertura **mostra** e non dove l'apertura sta, ed
è l'unico che rinuncia a una proprietà. Applicato in un solo posto —
`portal_view_origin`, l'origine dell'estremo di destinazione di
`portal_transform` — e da lì scende alla telecamera virtuale e al piano su cui
è tagliata la copia di un corpo.

**Cosa costa, misurato.** L'immagine non è più la fotografia che porterebbe un
buco nel muro: è quella fotografia presa un blocco più avanti. Non combacia più
col mondo ai bordi dell'apertura, e il gate di continuità non può più dimostrare
il contrario — confronta l'immagine con una telecamera vera **posta in quel
punto di vista**, quindi i suoi 0 pixel su 12960, 60726 e 242900 dicono che
l'offset è applicato in modo coerente, non che l'illusione è intatta. Detto qui
perché un gate verde che ha smesso di misurare la proprietà che misurava prima è
il modo più caro di sbagliarsi.

**Cosa guadagna, misurato.** Il salto dell'attraversamento scende da **0,80 a
0,20 nodi**, e il fotogramma d'arrivo differisce da quello mostrato in **23,56%
dello schermo invece che 29,24%**. Non è un caso: l'immagine e il corpo si sono
avvicinati. Con l'offset l'uscita senza salto non è più `exit = trigger` ma

    exit = trigger + view

cioè `1.10` con questi numeri — un decimo di nodo **fuori** dal blocco d'uscita.
È il valore che `config/portal.cfg` ora usa, ed è quello che ha chiuso il
cerchio: vedi 9.10.

**I riferimenti dei gate.** Ogni punto di vista di confronto in questo progetto
è derivato a mano dalle due basi e da una mezza rotazione, e ognuno di essi si
sposta con `view`: `with_view_offset` aggiunge l'offset lungo la normale della
destinazione, che è un asse che quelle derivazioni già nominano. Resta derivato
a mano — l'offset è un numero letto da un file, non la risposta di
`portal_transform` — quindi una mappa sbagliata in qualsiasi altro modo continua
a mancare il punto.

**Sabotaggio.** Con `portal_view_origin` riportato a `portal_plane_out` mentre
il file chiede `view = 1.00`, cadono **sei gate su ventitré**: `portal_math`,
`portal_crossing`, `portal_continuity`, `portal_cases`, `portal_orientation`,
`dual_session`, e la continuità stampa «no usable aperture to compare inside» a
tutte e tre le distanze. Con il solo piano della copia riportato indietro —
la mappa sposta il corpo, il taglio no — cade `portal_clip` nel modo esatto
previsto dal commento: 68921 punti su 68921 disegnati «di là», cioè il corpo
intero due volte.

**Il gate duale ha perso mordente, e non si è abbassata la soglia.** La seconda
misura di `dual_session` è il mondo A visto dallo stesso punto: gli unici pixel
che possono distinguere due mondi sono quelli che mostrano terreno, perché il
cielo è lo stesso cielo. Spostando il punto di vista in avanti l'apertura
inquadra più cielo e quella lettura è scesa da 29,56% a **19,07%**, sotto la
soglia del 20% — pur misurando un'immagine corretta (la prima lettura restava
0,00%). La risposta non è stata portare la soglia a 15: la vista è ora inclinata
di **venti gradi verso il basso**, l'apertura è piena di terreno, e la lettura
è **26,98%** su una regione di 68012 pixel invece di 61424.


### 9.10 L'uscita rimessa dove l'immagine la mostrava

Con il punto di vista spostato di un blocco, `exit = 0.90` non era più il numero
giusto: il salto residuo di 0,20 nodi era esattamente la distanza fra dove
l'immagine metteva l'occhio e dove il corpo veniva posato. Portato a

```
exit = 1.10
```

cioè `trigger + view`. Il controllo di intervallo su `exit` è stato allargato da
`[0, 1]` a `[0, 4]`, e non per far passare un numero: l'uscita senza salto è
`trigger + view`, che per qualunque offset di vista è **fuori** dal blocco
d'uscita. Un decimo di nodo davanti alla faccia d'arrivo è un posto, non un
errore.

Misurato, prima e dopo:

| | `exit = 0.90` | `exit = 1.10` |
|---|---|---|
| salto dell'attraversamento | 0,2000 nodi | **0,0000 nodi** |
| fotogramma d'arrivo vs. quello mostrato | 488476/2073600 (23,56%) | **9/2073600 (0,00%)** |
| scarto di posa alla cattura (`pose gap`) | — | eye 0,0000, forward 0,0000, up 0,0000 |
| continuità 6,5 / 3,0 / 1,5 blocchi | 0,00% | 0,00% |

Nove pixel su due milioni. L'attraversamento è una sola immagine: quello che il
giocatore stava guardando è quello in cui si trova il fotogramma dopo.

Va detto per intero: **questo non riporta l'illusione di 9.9 al suo posto.**
L'immagine resta la fotografia presa un blocco più avanti, e i bordi
dell'apertura continuano a non combaciare col mondo. Ciò che è tornato a zero è
l'altro costo — il salto — perché ora il corpo viene posato esattamente dove
l'immagine lo mostrava. Le due cose sono indipendenti e il file di config le
tiene su due righe separate.


### 9.11 Dov'è il piano di taglio obliquo — prima risposta, corretta in 9.12

> **Questa sezione arriva a una conclusione sbagliata e resta qui perché ci è
> arrivata con un ragionamento, non con una misura.** La risposta giusta, e come
> il giocatore l'ha trovata, sono in 9.12. Tutti i numeri qui sotto sono
> corretti; è la lettura che non lo è.

Domanda giusta al momento giusto: spostato il punto di vista di un blocco, il
piano vicino obliquo è ancora al posto suo?

**Sì, e l'ancoraggio è il motivo.** *(No: vedi 9.12.)* Il piano è agganciato al BLOCCO di
destinazione — `portal_plane_out - doorway_depth`, cioè la faccia lontana di
quel blocco — non alla telecamera. Spostare il punto di vista non lo muove,
perché quello che deve togliere di mezzo è geometria del mondo, non una
proprietà della vista.

Quello che è cambiato è **quando entra in funzione**: la guardia scatta solo
quando la telecamera virtuale sta dietro quella faccia, cioè quando il giocatore
è a più di `plane + view` = 1,5 nodi dall'apertura di partenza. Prima erano 0,5.
Fra 0,5 e 1,5 nodi ora non c'è alcun taglio obliquo — e non serve: la telecamera
è dentro o davanti al blocco di destinazione, e fra lei e l'apertura non c'è
niente. Misurato al limite: la continuità a 1,5 blocchi legge 0 pixel diversi su
242900.

**La regola più ordinata è quella sbagliata.** Con la mappa che ora incolla al
punto di vista, la regola «un vano dietro il piano incollato» darebbe
`view_origin - doorway_depth`, che con questi numeri cade esattamente sulla
faccia d'uscita — cioè la posizione che *era* il difetto della sessione scorsa.
Il piano è infinito: agganciarlo un blocco più avanti taglia via l'intero muro
della destinazione e il terreno su cui sta, e le facce laterali, interrogate su
cosa c'è accanto all'apertura, tornano indietro con il colore di sfondo.

**La scala, misurata** (pixel dipinti dall'apertura che sono il colore di sfondo
del mondo B, vista di tre nodi e uno e due di lato):

| piano di taglio | profondità alla destinazione | buchi |
|---|---|---|
| `plane_out - doorway` (allora in uso, oggi `clip = 2.00`) | −0,5 (faccia lontana del blocco) | **0 / 132045 (0,00%)** |
| `plane_out` = `view_origin - doorway` | +0,5 (faccia d'uscita) | 2235 / 129341 (1,73%) |
| `view_origin` (la finestra stessa) | +1,5 | 22044 / 129341 (17,04%) |

I due fotogrammi differiscono in 67607 pixel: è una differenza che il giocatore
vede, non un tecnicismo.

**E nessun gate esistente la vedeva.** Con il piano sulla finestra — 17% di
buchi — la continuità legge **0,00% a 6,5, 3,0 e 1,5 blocchi**. Non è una
sorpresa una volta detto: quel gate confronta il centro dell'apertura, rientrato
di un sesto per lato, e quello che il piano taglia è basso e vicino, sul bordo.
Per questo la misura nuova sta in `dual_session` sezione 3, è presa **di lato**,
e non usa nessun rettangolo: la regione sono i pixel che l'apertura ha
dipinto, trovati confrontando il fotogramma con lo stesso fotogramma senza
portale. Un rettangolo proiettato sbaglierebbe in entrambe le direzioni — a
quest'angolo contiene per lo più il muro del mondo A, e il cuneo che conta gli
sta appena fuori.

Sabotaggi, tutti e tre restaurati dopo la misura: piano sulla faccia d'uscita —
cade `dual_session`; piano sulla finestra — cadono `portal_math` e
`dual_session`. La suite resta 23/23 al posto giusto.


### 9.12 Lo z-fighting, e perché 9.11 aveva ragionato al contrario

Dal gioco: «ho visto z-fighting con il muro dell'altro mondo che dovrebbe essere
tagliato». Segnalazione giusta, e ribalta la conclusione della sezione
precedente.

L'argomento di 9.11 era che il piano appartiene al blocco di destinazione e
quindi non si muove con la vista. È sbagliato, e la geometria lo dice in una
riga. Con `view = 1` la mappa incolla il piano dell'immagine di partenza a un
piano **un blocco davanti** alla faccia d'uscita: il muro della destinazione,
che sta fra 0,5 e −0,5, è quindi **dietro la finestra**, e ciò che sta dietro la
finestra non si vede attraverso l'apertura. Va tagliato. Lasciandolo, viene
disegnato vicinissimo alla telecamera virtuale, le facce laterali lo mostrano, e
combatte per la profondità con la geometria su cui è dipinto — che è esattamente
quello che si vede giocando.

La regola giusta è quella generale, **un vano dietro il piano incollato**:

```
portal_clip_point = portal_view_origin - doorway_depth
```

Con `view = 0` questo è `plane_out - doorway_depth`, cioè la posizione che la
sessione scorsa aveva corretto: la regola non rinnega niente, la generalizza.
Scritta per esteso, la profondità alla destinazione è

    view + tunnel_start − 0,5      (nodi dal centro del nodo)

— `plane` si cancella, perché il vano sottratto è lo stesso vano in fondo al
quale sta l'immagine. `portal_math` verifica quell'aritmetica contro le
impostazioni, non contro sé stessa.

**Il cuneo arancione di 9.11 non era un buco.** Quei 2235 pixel erano il muro
della destinazione in un fotogramma e cielo nell'altro — cielo *dietro* il muro
rimosso, quindi contenuto legittimo. Il gate di 9.11 leggeva «via il muro» e lo
chiamava buco: misura giusta, interpretazione sbagliata, e la firma dell'errore
è che avevo ancorato la conclusione a un'argomentazione invece che a una
proprietà.

**Il gate ora misura entrambe le direzioni**, da un solo punto di vista di lato,
con due mire, contando il colore di sfondo del mondo B fra i pixel che
l'apertura ha dipinto:

| mira | un vano indietro | dov'è ora | un vano avanti |
|---|---|---|---|
| **in piano** (il muro coprirebbe il cielo dietro di sé) | 0,00% | **1,73%** | 17,04% |
| **in basso** (nessun cielo in inquadratura) | 0,00% | **0,00%** | 24,97% |

Il primo rigo chiede che il cielo *appaia*: se il muro è ancora lì, lo copre.
Il secondo chiede che non appaia niente: se il piano ha portato via anche il
terreno, appare. Sabotati entrambi i lati, cadono due gate per volta
(`portal_math` e `dual_session`).

La continuità legge **0,00% a tutte e tre le distanze in tutte e tre le
posizioni**, compresa quella che dipinge un quarto dell'apertura di colore di
sfondo. È la stessa lezione di [[gate-thresholds-hide-defects]] da un'altra
faccia: un gate che non può vedere una regione non dice niente su quella
regione, per quanto verde sia.


### 9.13 Il piano di taglio come impostazione

`config/portal.cfg` guadagna il sesto numero:

```
clip = 1.00
```

Quanto **dietro il piano incollato** — il punto di vista, `view` blocchi oltre
la faccia d'uscita — sta il piano vicino obliquo, in blocchi. Ancorato là e non
al blocco, così si muove con la vista: è la correzione di 9.12 resa esplicita
invece che sepolta in una formula.

La profondità alla destinazione, per esteso:

    (plane − 0,5) + view − clip      (nodi dal centro del nodo)

Il valore che tiene la regola è **`plane − tunnel_start`**, cioè un vano: è
quello che fa del tunnel che il giocatore percorre e del blocco di destinazione
lo stesso volume invece di due. Con questa forma è 1,00. A quel valore `plane`
si cancella e la profondità diventa `view + tunnel_start − 0,5`.

Qualsiasi altro valore è un esperimento voluto, e il motore lo **dice
all'avvio** invece di lasciarlo scoprire da un muro che z-fighta o da un bordo
che mostra cielo:

```
portal shape: NOTE clip is 0.00, not the 1.00 this doorway asks for. Short of it
the destination's ground is cut away with its wall; past it, the wall is left
standing in front of the opening.
```

I due lati, misurati come quota dei pixel dipinti dall'apertura che escono del
colore di sfondo del mondo B (`dual_session` sezione 3):

| mira | `clip = 0.00` | `clip = 1.00` | `clip = 2.00` |
|---|---|---|---|
| **in piano** (il muro coprirebbe il cielo dietro di sé) | 17,04% | **1,73%** | 0,00% |
| **in basso** (nessun cielo in inquadratura) | 24,97% | **0,00%** | 0,00% |

Sotto il vano il piano sta addosso al punto di vista e taglia troppo — se ne va
anche il terreno della destinazione; sopra il vano il muro resta in piedi. Il
gate chiede che in piano il cielo **appaia** e che in basso **non appaia
niente**, e `portal_math` verifica l'aritmetica contro le impostazioni. Con
`clip` fuori posto cadono due gate in entrambe le direzioni.

### 9.14 Le sei impostazioni, in una tabella

| | | valore | cosa costa se lo cambi |
|---|---|---|---|
| `plane` | dove sta l'immagine dentro il blocco | 1,00 | a 0,5 il vano non ha profondità e non ci sono facce laterali |
| `tunnel_start` | dove cominciano le facce laterali | 0,00 | oltre `plane` il vano è rovesciato: rifiutato |
| `trigger` | quanto dentro deve entrare l'occhio per attraversare | 0,10 | sposta il punto d'innesco, e con esso l'uscita senza salto |
| `exit` | dove viene posato il giocatore all'arrivo | 1,10 | ogni centesimo lontano da `trigger + view` è un centesimo di nodo di salto |
| `view` | quanto oltre la faccia d'uscita si guarda | 1,00 | a 0 l'immagine è un portale vero ma mostra il vano d'arrivo dall'interno |
| `clip` | quanto dietro il punto di vista sta il piano vicino | 1,00 | sotto il vano si taglia il terreno, sopra resta il muro |

Le prime quattro sono la forma del blocco, `view` è cosa mostra, `clip` è cosa
nasconde. Le ultime due sono quelle che si sono dovute misurare due volte.


---

## 10. Quanto costa il secondo mondo

§4.2 dichiara «renderizzare il mondo B costa il 3-7% del frame» e conclude che
«il multi-sessione non è il problema di performance». Sono due affermazioni sul
renderer del fork. Qui non era stato misurato niente, ed era scritto in §7. Ora
c'è `cnp_client --dual-bench`.

Il metodo, sulla stessa scena che `dual_session` verifica — costruita dalla
stessa funzione, perché un benchmark che si costruisce la propria
approssimazione della scena misura la propria approssimazione:

- 900 frame per passata, ogni passata **due volte**, interlacciate; si tiene la
  lettura minima e si stampa lo scarto fra le due. Un tempo di frame ha un
  pavimento e non un soffitto: quello che la macchina fa d'altro può solo
  aggiungere.
- **presentazione spenta e `gfx_finish()` a ogni frame.** Senza il recinto la
  stessa scena legge 0,166 ms invece di 0,334: si ferma il cronometro quando il
  driver *accetta* il lavoro, non quando è fatto. Il soak lo sapeva già; il
  primo giro di questo bench no, e la differenza è un fattore due.
- telecamera ferma a tre nodi dall'apertura, entrambi i mondi completamente
  meshati prima di cronometrare.

### 10.1 I numeri

Mondo 512 (`--dual-bench --world 512`), due esecuzioni separate del processo:

| | run 1 | run 2 |
|---|---|---|
| un frame con il portale | 0,391 ms (2558 FPS) | 0,394 ms (2539 FPS) |
| un frame senza portale | 0,212 ms | 0,191 ms |
| con il view range passivo tolto | 0,681 ms | 0,658 ms |
| il portale che mostra **questo** mondo | 0,694 ms | 0,661 ms |
| → **il portale costa** | 0,179 ms (45,8% del frame) | 0,203 ms (51,6%) |
| → il view range passivo fa risparmiare | 0,290 ms (61,8% del costo del portale) | 0,264 ms (56,5%) |
| → **quale** mondo c'è nel target vale | 0,013 ms | 0,003 ms |

Mondo 128, il default: portale 0,334 ms contro 0,173 senza, cioè **0,161 ms e
il 48%**; il view range passivo risparmia −0,006 ms, cioè niente.

### 10.2 Cosa dicono

**«Il multi-sessione non è il problema di performance» è confermato, e per una
ragione più netta di quella del progetto.** Quale mondo sta nel render target
vale **0,003 ms**: sotto il rumore. Il portale che mostra il mondo *dell'altra
sessione* e il portale che mostra il *proprio* costano la stessa cifra. Quello
che costa è disegnare un mondo **una seconda volta**, e quanto se ne disegna.
Due mondi vivi non sono più cari di uno stesso mondo guardato due volte.

**Il 3-7% non si trasferisce, e non perché il portale costi di più.** Qui il
portale è **circa metà del frame** — ma il frame senza portale è 0,19 ms. In
valore assoluto il portale costa 0,16-0,21 ms su un criterio di 5 ms (200 FPS).
La percentuale del fork è una frazione di un frame molto più grasso: citarla
senza il denominatore è la cosa che questo numero serve a smettere di fare.

**Il view range ridotto è l'unico item del budget passivo implementato, e si
paga da solo — quando il range morde.** Su un mondo da 512 nodi risparmia
0,26-0,29 ms, il **57-62%** di quello che il portale costerebbe altrimenti. Sul
mondo da 128 non risparmia niente, ed è ovvio una volta detto: un terzo di 512
è 171 nodi, e un mondo largo 128 ci sta tutto dentro. La misura sul default non
è un risultato negativo, è **vacua**, e va letta come tale.

**Il tick di mesh passivo, in numeri invece che in «qualche secondo»:** un
chunk per tick rimesha il mondo passivo intero in **32 tick (0,5 s a 60 fps)**
su 128 nodi e **512 tick (8,5 s)** su 512. Per questo la scena `--dual` fa il
mesh completo una volta sola all'avvio e lascia al tick solo le modifiche.

### 10.3 La prossima leva, misurata prima di tirarla

> **Tirata e misurata in 10.4.** Il paragrafo qui sotto resta com'era scritto,
> perché la previsione che conteneva — «una parte è fill rate su pixel che
> finiscono dentro un'apertura larga poche centinaia di pixel» — è
> **sopravvalutata**, e i numeri lo dicono meglio di una riscrittura.

§4.2 chiede anche il render target **alla risoluzione del portale** invece che
dello schermo. Oggi il target è 1920×1080 come la finestra, e il portale costa
0,16-0,21 ms di cui una parte è fill rate su pixel che finiscono dentro
un'apertura larga poche centinaia di pixel. È l'unico item del budget rimasto
che possa spostare un numero — audio e particelle non esistono, la predizione
non c'è — e ha un costo di qualità da misurare a sua volta: il quad campiona il
target per posizione sullo schermo, quindi un target più piccolo è un'immagine
più morbida. Dichiarato, non fatto.


### 10.4 Il target alla risoluzione del portale: quanto rende, quanto costa

L'ultimo item del budget di §4.2 che potesse ancora spostare un numero, adesso
implementato (`--portal-scale F`) e misurato in tutte e due le direzioni nello
stesso posto — perché sceglierne il default sul solo costo sarebbe sceglierlo
con metà dei numeri.

L'apertura, dal punto in cui un giocatore le arriva davanti, dipinge **171469
pixel su 2073600: l'8,3% dello schermo**. Il target che la alimenta era grande
quanto la finestra.

| scala | target | ms | risparmio | pixel cambiati | errore medio |
|---|---|---|---|---|---|
| 1,00 | 1920×1080 | 0,392 | — | — | — |
| 0,75 | 1440×810 | 0,366 | 0,027 | 12,2% | 3,9 |
| 0,50 | 960×540 | 0,357 | **0,036** | **18,1%** | 6,0 |
| 0,35 | 672×378 | 0,338 | 0,054 | 18,3% | 6,6 |
| 0,25 | 480×270 | 0,332 | 0,061 | 23,5% | 7,5 |

«Pixel cambiati» sono quelli **dell'apertura** che si spostano di più di 8 su
255 in un canale, contro l'immagine a piena risoluzione; «errore medio» è quello
spostamento mediato su tutta l'apertura. Il denominatore è la maschera dei pixel
davvero dipinti dall'apertura, trovata diffando il fotogramma contro lo stesso
fotogramma senza portale — non una scatola proiettata.

**Il verdetto è che la leva è debole qui, e la previsione di 10.3 era
sopravvalutata.** Il portale intero costa 0,212 ms; dimezzare la risoluzione ne
recupera **0,036**, cioè il **17% del costo del portale e il 9% del fotogramma**,
in cambio del **18% dei pixel dell'apertura visibilmente cambiati**. Il motivo è
misurabile nella stessa tabella: scendere da 0,50 a 0,25 — un quarto dei pixel —
risparmia altri 0,025 ms soli. Quello che il passaggio nel target costa non è
fill rate, è **geometria**: 1,47 milioni di vertici del mondo B, che vengono
disegnati lo stesso qualunque sia la dimensione del bersaglio.

**Default lasciato a 1,00**, e la bandiera esiste per chi ha un fotogramma
diverso dal mio: su una GPU dove il fill rate conta di più (risoluzione alta,
mondo povero di vertici) la stessa tabella dirà un'altra cosa, e adesso c'è il
comando che la stampa.

Il meccanismo da cui dipendono quei numeri è a sua volta sotto gate, perché una
scala che non facesse niente renderebbe la tabella sopra un elenco di rumore:
`dual_session` sezione 9 verifica che il target passi davvero da 1920×1080 a
960×540 e che l'immagine dentro l'apertura sopravviva (169246 pixel dipinti
contro 168371, cioè lo 0,5% di differenza — più morbida, non sparita).
Sabotaggio, con `portal_rt_dim` che ignora la scala: il target resta 1920×1080 e
il gate diventa rosso.

---

## 11. Il teleport che ogni tanto non succede

Dal gioco: «ogni tanto il teleport attraverso il portale fallisce, ogni tanto il
teleport mi incastra in un blocco di uscita». Intermittente in entrambi i sensi,
e tutti i gate verdi — per lo stesso motivo di §8: entravano tutti in mezzo,
a un'altezza, a una velocità, in una direzione. Un percorso attraverso
un'apertura non dice niente sugli altri.

### 11.1 Lo sweep

`dual_session` sezione 7: offset laterali ogni quarantesimo di nodo su tutto
quello che la scatola di collisione riesce a infilare, tre modi di avvicinarsi
(dritto, in diagonale mirando al centro, in volo), e **ogni caso attraversato
nei due sensi** — andata e ritorno, perché il ritorno atterra nel terreno
dell'altro mondo e metà degli attraversamenti che fa un giocatore sono ritorni.
Guidato dal percorso vero del client, `player_update` poi `step_portals`, con
ogni esito contato: **crossed / refused / buried / wedged / short**.

Prima misura, 273 avvicinamenti: **7 rifiutati**. Il sintomo esatto — il
giocatore attraversa il buco e esce dall'altra parte dello stesso muro, senza
che niente glielo dica.

### 11.2 La causa: due regole per «sei dentro un blocco»

Due funzioni rispondono alla stessa domanda per due pubblici diversi.
`box_hits_world` decide **dove un giocatore può camminare** — è quella che lo
mette dov'è. `inside_solid` decide **quali posizioni sono accettate**: il
validatore di movimento del server, e l'arrivo di un portale.

Arrotondavano la faccia lontana della scatola in modo diverso:

```
box_hits_world   x1 = floor(max.x - kBoxSkin)   il bordo sul confine tocca il nodo dietro
inside_solid     x1 = floor(x + kHalfWidth)     il bordo sul confine entra nel nodo davanti
```

Un giocatore poteva quindi **camminare** in un punto che `inside_solid`
chiamava «dentro la pietra». Con un'apertura larga un nodo e una scatola larga
sei decimi ci sono due decimi di gioco per lato, e il bordo cade esattamente sul
confine: entra a −0,200 di lato, l'arrivo è a −0,200 dall'altra parte, e il
bordo della scatola tocca il muro accanto al buco. Rifiutato.

Il commento in cima a `movement.cpp` aveva previsto la forma esatta del guasto,
per due costanti invece che per due regole: *«when they disagree the server
refuses positions the player reached legally, which is the worst failure this
code has»*. Mancava il controllo sotto quel commento.

**Correzione:** `kBoxSkin` diventa pubblico in `aabb.h` e `inside_solid` usa la
stessa regola sul bordo lontano. Il verticale resta più permissivo (un
ventesimo di nodo a piedi e testa) ed è documentato perché: è quello che
accetta una posizione appena assestata nel pavimento.

**Gate nuovo, `physics_box`:** le due funzioni confrontate su **989901
posizioni** attraverso una porta larga un nodo e attorno a un blocco isolato,
più il caso di confine derivato a mano. Zero disaccordi. Sabotato rimettendo
l'arrotondamento vecchio: **12928 posizioni** in disaccordo, il primo a
10,700 4,000 11,000.

### 11.3 La seconda causa: l'arrivo si risolveva solo verso l'alto

Con l'arrivo occupato — terreno un nodo più alto, un blocco piazzato, una cassa
— `portal_traverse` provava solo a **salire** di nodi interi, fino a tre, e poi
rifiutava. Ma `exit = 1.10` mette l'occhio un decimo di nodo fuori dal blocco di
destinazione, e la scatola è larga tre decimi: **due decimi di scatola sono
sempre dentro quel blocco**, e l'unica ragione per cui è legale è che a
quell'altezza c'è l'apertura. Alzarsi di un nodo mette quei due decimi contro il
**muro accanto al buco**, e ogni passo in su viene rifiutato per un motivo che
non c'entra niente con l'ostacolo.

Misurato: un blocco solo sull'arrivo di un portale in un muro vero, e
l'attraversamento **rifiutato**, sempre.

**Correzione:** l'arrivo si risolve **prima fuori dal vano, poi in su**. I
candidati, nell'ordine in cui un giocatore li accetterebbe:

| | |
|---|---|
| così com'è | non c'era niente di mezzo |
| spinto fuori | abbastanza avanti da portare la scatola fuori dal blocco di destinazione, mantenendo altezza e posizione laterale che l'immagine mostrava |
| l'uno o l'altro, un nodo più su (×3) | uno scalino, o terreno più alto dall'altra parte; oltre tre è un muro e la destinazione deve rifiutare invece di inghiottire |

Lo spostamento va tutto nel `warp_offset`, non solo la parte verticale: una
spinta laterale che scatta è visibile quanto una verticale.

**Gate nuovo, `dual_session` sezione 8:** un blocco dove atterra l'arrivo, due
volte — in un portale con cielo aperto sopra (dove il passo in su bastava già) e
in un portale **dentro un muro** (dove non bastava). Sabotato togliendo la
spinta: il secondo caso torna rifiutato.

### 11.4 Un gate che passava per il difetto sbagliato

La sezione «destinazione murata» riempiva due nodi e basta — che non è una
destinazione murata, sono due blocchi con aria aperta sopra, cioè uno scalino.
Passava perché il risolutore dell'arrivo **non poteva usare quell'aria**: ogni
passo in su era rifiutato dal muro, non dall'ostacolo. Corretto il risolutore,
il caso è diventato verde nella direzione sbagliata — il giocatore veniva messo
in cima ai due blocchi, correttamente. Ora l'ostacolo sale oltre ogni candidato
che il risolutore prova, che è quello che «murata» ha sempre voluto dire.

### 11.5 Cosa NON è stato riprodotto, detto

Il secondo sintomo — «mi incastra in un blocco di uscita» — **non è stato
riprodotto**. Il meccanismo corretto in 11.3 lo produce (il passo in su
scaraventava il giocatore su un nodo dentro il muro accanto al buco, ed è
esattamente la sensazione descritta), ma nessuna delle 110 traversate dello
sweep e nessuno dei casi con ostacolo lo ha mai mostrato. Lo sweep **conta**
quell'esito — atterrato fuori dai solidi ma incapace di muoversi di mezzo nodo
in due secondi — e legge 0. Resta in `TODO.md` finché non c'è un caso che lo
mostri: «sembra risolto» non è un motivo per cancellare una riga.


---

## 12. La scena `--oriented`, dove il difetto era peggiore

Dal gioco, subito dopo §11: «il secondo caso mi è successo nel mondo oriented.
inoltre mi succede che la direzione della vista mi scatti di colpo dopo il
teleport nel modo oriented».

Quella scena è una coppia in cui la destinazione è **coricata**: il suo buco è
largo due nodi e alto uno. Il commento della scena lo diceva già — «esattamente
la forma che un giocatore in piedi non può attraversare» — e il gate del roll
(caso 6) **scava il terreno oltre la destinazione** apposta, per misurare il
roll invece della ricerca dell'arrivo. Nessun gate misurava l'arrivo in quella
scena, che è l'unica cosa che un giocatore ci fa dentro.

### 12.1 Cosa succedeva davvero

Nuovo gate, `portal_orientation` sezione 10: la coppia della scena `--oriented`
**con entrambe le lastre al loro posto**, percorsa da sei pose diverse.

| | prima (solo passo in su) | dopo (fuori dal vano, poi in su) |
|---|---|---|
| attraversamenti riusciti | **0 su 4** | **4 su 4** |
| rifiutati | 4 | 0 |
| dentro un solido / incastrati | 0 | 0 |
| warp massimo della telecamera | — | **0,23 nodi** |

In quella scena l'attraversamento **non funzionava affatto**: il giocatore
entrava nell'apertura e non succedeva niente, da ogni posa provata. È la stessa
causa di §11.3 nella sua forma peggiore — l'arrivo sta due decimi dentro il
blocco di destinazione, il buco è alto un nodo, quindi la scatola è nella lastra
sopra e sotto, e salire di nodi interi non ne esce mai.

### 12.2 E lo scatto della vista

Il risolutore vecchio poteva solo **salire di nodi interi**, fino a tre. Quando
ci riusciva, consegnava alla telecamera un warp di quella misura: un salto di
uno o tre nodi assorbito in un sesto di secondo. La vista parte da una parte e
arriva da un'altra — ed è quello che si vede.

Il gate ora misura la cosa direttamente: **il warp consegnato alla telecamera
deve stare sotto mezzo nodo**. Con la correzione è 0,23 — la spinta fuori dal
vano più l'assestamento. Sabotando la spinta, i quattro attraversamenti tornano
tutti rifiutati e il gate cade sulla prima riga.

### 12.3 E la vista fotogramma per fotogramma

`portal_orientation` sezione 9, aggiunta per cercare lo scatto dove non l'ho
trovato: la coppia `--oriented` percorsa da **nove pose** (in volo, a piedi,
guardando su e giù fino a ottanta gradi, entrando di sbieco), misurando quanto
gira la vista **fra un fotogramma e il successivo** per un secondo e mezzo dopo
l'attraversamento.

- direzione dello sguardo: **al massimo 0,02 gradi per fotogramma**
- orizzonte: **3,00-3,15 gradi per fotogramma**, che è l'animazione del roll a
  mezzo giro al secondo, esattamente com'è progettata
- attraversamenti in più dopo il primo: **0** — nessun rimbalzo indietro

Quindi lo scatto non è nella direzione dello sguardo né nel roll: è il warp di
posizione di 12.2. Detto per intero perché la prima ipotesi era sbagliata e la
misura l'ha scartata prima che diventasse una correzione.

### 12.4 Lo scatto succede ancora, e lo strumento per vederlo

Riprovato dopo la correzione: **«il problema dello scatto della direzione della
visuale succede ancora»**. Quindi 12.3 non è la spiegazione completa. Vale la
pena dire perché quella misura può essere verde mentre il difetto è vivo: la
sezione 9 guida il giocatore con un input **scritto nel gate** — avanti,
frequenza fissa a 60 fotogrammi al secondo, mouse fermo. Chi gioca ha un mouse
vero, un tempo di fotogramma vero, e stava facendo qualcosa che il gate non fa.

Invece di indovinare la nona ipotesi, `--log-view` stampa **cosa ha fatto la
telecamera**, una riga per fotogramma, per due secondi attorno a ogni
attraversamento, più i sei fotogrammi che lo precedono (tenuti in un anello:
quando l'attraversamento scatta sono già passati).

La colonna che decide è `unexplned`: l'angolo fra la direzione dello sguardo di
questo fotogramma e quella che **gli angoli del fotogramma precedente più il
mouse di questo** avrebbero prodotto. Un mouse può girare la vista quanto una
mano riesce a muoversi, e non è un difetto; una rotazione che nessuno ha
chiesto lo è. Accanto ci sono le altre due cose che possono muovere una vista
senza che gliel'abbiano chiesto: `clamp`, il limite di beccheggio a 89 gradi —
che morde esattamente dove un'uscita da un'apertura coricata lascia lo sguardo
— e `horizon`, l'animazione del roll.

```
view f+  0 CROSS w1 dt  16.7ms mouse   +0.0   +0.0 roll  -90.00 turn 118.24 ...
view f+  1       w1 dt  16.7ms mouse  -12.0   +3.0 roll  -87.00 turn   1.79 asked   1.79 unexplned  0.003 clamp  0.00 horizon   3.00 ...
```

**Lo strumento è a sua volta misurato**, perché una traccia che legge zero
durante uno scatto scagionerebbe il codice colpevole. `portal_orientation`
sezione 11 le dà quattro fotogrammi costruiti a mano:

| fotogramma | `turn` | `asked` | `unexplned` | `clamp` | `horizon` |
|---|---|---|---|---|---|
| solo mouse, 40 pixel | 5,73 | 5,73 | **0,000** | 0,00 | 0,00 |
| lo stesso con 30 gradi di scatto iniettati | 35,73 | 5,73 | **30,00** | 0,00 | 0,00 |
| mouse in su contro il limite di beccheggio | — | — | **0,000** | **56,72** | 0,00 |
| roll che decade di 3 gradi | 0,000 | — | **0,000** | 0,00 | **3,00** |

Cioè: legge uno scatto vero come 30 gradi, e non ne inventa uno né quando è il
limite di beccheggio a muovere la vista né quando è l'animazione del roll.

**Limite dichiarato:** il difetto **non è ancora localizzato**. Questa non è la
correzione, è lo strumento che dirà quale fotogramma incolpare — e finché non
arriva una riga con `unexplned` grande, non c'è niente da correggere sulla base
di un'ipotesi.

### 12.5 La domanda sul cubo, misurata

Chiesto, appena la traccia era dentro: **«può essere che lo scatto della
telecamera accada ogni volta che il cubo che va avanti e indietro interagisce
col portale?»** È la forma giusta di domanda: il cubo incontra l'apertura su un
ciclo di undici secondi, e una cosa che succede ogni undici secondi è
esattamente quello che un giocatore nota e che un gate fatto di corse da due
secondi non vede.

Nel codice i corpi arrivano al renderer e a nient'altro — `draw_bodies` e
`portal_cut` li leggono, nessuna riga di fisica. Ma quella è un'affermazione sul
codice e il rapporto viene dal gioco, quindi è stata misurata.
`portal_orientation` sezione 12 fa **la stessa camminata attraverso lo stesso
attraversamento due volte**: una col cubo che scorre nell'apertura esattamente
come lo anima il client, una senza cubo, e confronta i due flussi di pose
fotogramma per fotogramma.

```
400 fotogrammi, il cubo in due mondi insieme su 59 di essi; 1 attraversamento con e 1 senza
col cubo contro senza: posizione 0.000000, angoli 0.000000, warp 0.000000;
peggiore rotazione non richiesta 0.0000 gradi
```

**Zero esatto**, e i 59 fotogrammi di taglio dicono che il caso interessante è
stato davvero attraversato invece che saltato.

Sabotaggio, perché uno zero senza sabotaggio non è una misura: aggiunta in
`update_floor_shift` una riga che fa spingere il giocatore dal cubo quando è a
meno di 1,5 nodi (`player.yaw += 0.02f`, cioè quello che farebbe una collisione
corpo-giocatore scritta male). Il gate diventa rosso:

```
posizione 0.145312, angoli 0.280000, warp 0.120001; peggiore rotazione non richiesta 1.1460 gradi
```

Quindi: **no, non è il cubo** — oggi. Il gate resta lì perché la risposta cambia
il giorno in cui i corpi entrano nella fisica, e allora lo dirà da solo.

Aggiunta comunque una colonna `crate` alla traccia: distanza del cubo dal piano
dell'apertura lungo la normale, con `CUT` quando è in due mondi insieme. Se lo
scatto arriva mentre quel numero passa per lo zero, si vede nella riga.

### 12.6 La traccia era cieca per costruzione: 36 millisecondi

Il primo log tornato dal gioco ha detto una cosa che nessun gate aveva mai
misurato: **`dt 0.30ms`**. Il client gira senza limite di fotogrammi e senza
sincronia verticale (`plat_delta_time` è `GetFrameTime()` e non c'è nessun
`SetTargetFPS`), e in quella scena fa circa **3300 fotogrammi al secondo**.

Conseguenza: la finestra da **120 fotogrammi** che la traccia apriva dopo ogni
attraversamento era larga **36 millisecondi**. Si chiudeva prima che
l'animazione del roll avesse fatto un decimo del suo giro, e un giocatore che
vede la vista scattare mezzo secondo dopo non avrebbe trovato niente nel log.
Lo strumento era cieco esattamente nell'intervallo per cui era stato costruito —
e c'è voluto un log vero per dirlo. Nel log iniziale il roll decade di 0,045
gradi per fotogramma invece dei 3,00 che il gate misura a 60 fotogrammi: stessa
animazione, stessa velocità al secondo, un fotogramma cinquanta volte più corto.

Corretto: la finestra è **due secondi**, non un numero di fotogrammi, e le righe
ordinarie sono campionate una ogni 4 ms perché stamparne 3300 al secondo
sotterra il log. Un fotogramma che attraversa o che scatta non viene mai
campionato via — la misura gira su ogni fotogramma, solo il racconto è diradato.

`portal_orientation` sezione 13 lo verifica alle due frequenze:

| fotogramma | righe | finestra chiusa a |
|---|---|---|
| 16,67 ms (60 al secondo) | 120 | 2,017 s |
| 0,30 ms (3300 al secondo) | 476 | 2,000 s |

La versione a conteggio di fotogrammi avrebbe dato 120 righe in entrambi i casi,
cioè 36 ms nel secondo: è quel numero che il gate rifiuta adesso.

**Da notare a parte:** che il client giri senza tetto di fotogrammi è un fatto
misurato qui e non una scelta documentata altrove. Tutta l'animazione dipende da
`dt`, quindi non è un difetto di per sé, ma è il genere di cosa che va sotto gli
occhi prima di M6.

### 12.7 Il secondo log: sette attraversamenti, zero segnalazioni, e perché

Riprovato dal vivo con la traccia corretta di 12.6: **sette attraversamenti,
3501 righe, zero scatti segnalati** — eppure lo scatto c'era. Il log dice tre
cose, e la terza è un altro difetto dello strumento.

**Uno. La direzione, dentro le finestre, non si muove da sola.** Su ogni riga
non di attraversamento `unexplned` è esattamente 0,00 e la rotazione più grande
è **4,51 gradi**, tutti chiesti dal mouse. Il `clamp` non morde mai.

**Due. Il warp della telecamera in gioco vero è tre volte quello del gate.**

| attraversamento | warp | direzione |
|---|---|---|
| 2 | **0,651** | verticale |
| 6 | **0,692** | verticale |
| 4 | 0,277 | verticale |
| 1, 3, 7 | 0,201-0,232 | orizzontale |

Il gate della sezione 10, con le sue sei pose scritte a mano, misurava **0,23**
come caso peggiore contro una soglia di 0,5. Il gioco vero ne produce **0,692**:
la soglia non è stata superata perché le pose del gate non arrivano dove arriva
un giocatore. Un warp di 0,69 nodi assorbito in 0,15 s è un occhio che sale di
due terzi di blocco in un sesto di secondo — cioè uno scatto, anche se la
direzione dello sguardo non si muove di un grado. **Le pose del gate vanno rese
rappresentative, non la soglia più larga.**

**Tre. La regola dello scatto era una frequenza di fotogrammi travestita.**
Diceva «più di un grado non richiesto in UN fotogramma». A 0,30 ms per
fotogramma quello vuol dire **tremila gradi al secondo**. Uno scatto di trenta
gradi in un decimo di secondo — all'incirca il più veloce che un occhio fonde in
un salto solo — arriva come trecento fotogrammi da un decimo di grado e non fa
scattare niente.

Corretto: i gradi non richiesti vanno in un secchio che **perde in un decimo di
secondo**, e la soglia è 12 gradi dentro quella finestra. `portal_orientation`
sezione 14 gli dà esattamente quello scatto a esattamente quel tempo di
fotogramma:

```
30 gradi su 333 fotogrammi da 0,30 ms: 0,093 gradi nel fotogramma peggiore, 18,53 nel secchio
```

Cioè: la regola vecchia vede un decimo di grado e tace, la nuova lo chiama
scatto. Sono le due metà della stessa verifica e ci sono tutte e due.

**Altre due cose che il log ha cambiato nello strumento:**

- Una riga stampata adesso **riassume tutto il gruppo di fotogrammi che
  rappresenta** (gradi sommati, mouse sommato, clamp peggiore, `n` fotogrammi):
  a 3300 al secondo se ne stampa una su tredici, e prima le dodici saltate
  potevano contenere il difetto.
- Il mouse è stampato **due volte**: `raw` com'è arrivato dalla mano e `mouse`
  dopo `look_apply_roll`. Mentre l'orizzonte è inclinato sono due vettori
  diversi, e una vista che va dove la mano non ha chiesto verrebbe altrimenti
  addebitata al mouse. Nel log: raffiche da 21 a 32 pixel in un fotogramma da
  0,26 ms, perché il mouse riporta a 125-1000 Hz e il client gira a 3300.

### 12.8 Il rapporto, detto esatto: la vista torna alla direzione d'uscita

Il rapporto si è fatto preciso, ed è un'altra cosa da quello che stavo
misurando: **«dopo alcuni attraversamenti, ogni tanto la direzione della
telecamera scatta verso la direzione di uscita dell'ultimo teletrasporto»**. Non
«la vista si muove»: la vista **torna dove l'attraversamento l'aveva lasciata**,
molto dopo l'attraversamento.

**Chi può scriverla, quella direzione.** Nel ciclo interattivo `player.yaw` e
`player.pitch` vengono scritti da tre punti soli: `apply_traversal`, il tasto
`R` e la rete di sicurezza sotto il mondo. Gli ultimi due mettono
`spawn_yaw` — la direzione dello spawn, non quella d'uscita — e adesso lo dicono
a voce tutti e due. Quindi **una vista che salta alla direzione d'uscita
dell'ultimo teletrasporto viene da un attraversamento**, sentito o no.

La traccia adesso lo misura invece di dedurlo. Al momento di ogni
attraversamento si tiene la direzione d'uscita; ogni riga porta due colonne
nuove:

- `toexit` — gradi fra dove si guarda adesso e dove l'attraversamento aveva
  lasciato lo sguardo
- `closing` — quanti di quei gradi si sono chiusi dentro un decimo di secondo

Una vista riportata indietro chiude tutto il divario in un colpo; un giocatore
che si rigira col mouse lo chiude piano. `portal_orientation` sezione 15 misura
tutte e due:

| caso | `closing` | segnalato |
|---|---|---|
| rimessa dov'era in un fotogramma | **40,00** | sì |
| rigirata a mano in un secondo | 4,01 | no |

Aggiunte anche due segnalazioni che mancavano, perché la traccia era muta
esattamente dove serviva:

- `FAST TURN` — più di 25 gradi in un decimo di secondo **chiunque li abbia
  chiesti**. Prima una rotazione addebitata al mouse non stampava niente, e una
  raffica accumulata dal mouse è indistinguibile da un difetto finché non se ne
  vedono i pixel accanto.
- **Un battito ogni mezzo secondo** anche quando non succede niente. Con le sole
  finestre attorno agli attraversamenti il log era fatto di isole: «è scattato
  qualche secondo dopo» non era verificabile perché di quei secondi non c'era
  nessuna riga. Due righe al secondo non costano niente.

### 12.9 Era il warp del puntatore, e non scatta più

**Trovato.** Afferrare il cursore sposta il puntatore al centro della finestra, e
la piattaforma riporta quello spostamento come delta del mouse nei fotogrammi
successivi. Ne venivano scartati due **all'avvio e mai più**. Quindi ogni `ESC`
per liberare il mouse e `ESC` per riafferrarlo lanciava la vista di quanto il
puntatore aveva vagato mentre era libero.

Spiega ogni pezzo del rapporto che nessuna delle mie ipotesi spiegava:

- **«ogni tanto»** — solo quando si esce e si rientra dal mouse, non a ogni
  attraversamento
- **«dopo alcuni attraversamenti»** — è il momento in cui si molla il mouse per
  guardare qualcosa, e si riprende
- **«verso la direzione di uscita dell'ultimo teletrasporto»** — il puntatore
  vaga verso il centro dello schermo, e riprendendolo la vista viene riportata
  indietro **verso dove stava guardando prima di mollarlo**, che dopo un
  attraversamento è la direzione d'uscita
- **perché nessun gate l'ha mai visto** — nessun gate preme `ESC`

Misura del difetto: con il puntatore vagato di 1400 per 620 pixel, **2050 pixel
arrivavano alla vista**. A 0,0025 radianti per pixel sono **5,1 radianti**, cioè
la vista buttata di quasi 300 gradi in un fotogramma.

**Corretto** rendendo lo scarto una cosa con un nome invece di un intero dentro
un ciclo — `LookSettle` in `client/player.h` — perché il difetto era esattamente
un intero riarmato in un posto e non nell'altro: i due cicli del client avevano
la stessa riga e nessuno dei due la riarmava. `portal_orientation` sezione 16 lo
verifica, sabotaggio compreso:

| | pixel arrivati alla vista |
|---|---|
| con lo scarto riarmato a ogni presa | **0** |
| con `grabbed()` sabotato a non riarmare | **2050** |

**Confermato dal gioco.** Sessione dopo la correzione: **14 attraversamenti,
6775 righe, zero eventi di qualunque tipo.** Il peggio misurato in tutta la
sessione:

| | peggiore | soglia |
|---|---|---|
| gradi non richiesti in un decimo di secondo | **5,55** | 12 |
| divario chiuso verso la direzione d'uscita | **15,53** | 30 |
| raffica grezza del mouse in un fotogramma | **42 px** | 300 |

(Il `unexplned 119.96` che compare nel log è il fotogramma dell'attraversamento
stesso: quello è il portale.) Riferito da chi gioca: **«non lo vedo più
scattare»**.

**Resta aperto** il warp della telecamera, che in questa stessa sessione ha
toccato **1,216 nodi** — `TODO.md` OPEN-4, ed è un'altra cosa: la telecamera che
trasla, non la direzione che gira.

### 12.10 Il warp della telecamera: misurato dove va un giocatore, e limitato

Il difetto rimasto da 12.7: la telecamera si mangiava fino a **1,253 nodi in
0,15 secondi** — 8,3 nodi al secondo, il doppio di uno scatto — mentre la
sezione 10, con le sue sei pose scritte a mano, leggeva 0,23 contro una soglia
di 0,5. Verde perché la misura era stretta, non perché il motore fosse gentile.

**Prima le pose.** `portal_orientation` sezione 17 non elenca gli avvicinamenti,
li spazza: cinque scostamenti attraverso l'apertura, cinque modi di arrivarci
(a piedi, a piedi saltando, in volo piano, in volo guardando su e giù), nelle
due direzioni. Cinquanta avvicinamenti, cinquanta attraversamenti.

```
peggiore spostamento 1,420 nodi (a piedi, +0,8 di lato)
```

Più di quello che il gioco aveva prodotto. E lo spostamento **è geometria, non
un difetto**: le due aperture di `--oriented` stanno mezzo nodo una più in alto
dell'altra, quindi un attraversamento in un verso mette sempre i piedi nel
pavimento; e la destinazione è coricata, quindi entrare 0,8 **di lato** vuol dire
uscire 0,8 **in basso**. Uscire dal pezzo di apertura in cui si è entrati è
quello che deve succedere.

**Quello che si sente è la velocità.** Il tempo di recupero era fisso a 0,15 s,
sul ragionamento che un sollevamento di due centimetri e uno di un nodo dovessero
finire nello stesso batter d'occhio. Adesso è una distanza a velocità limitata:

```
tempo = clamp(distanza / 2,5 nodi al secondo, 0,15 s, 0,60 s)
```

2,5 nodi al secondo è appena sotto una camminata (4,3): la telecamera che sale
più piano di quanto il giocatore l'avrebbe camminata è quello che la fa leggere
come un arrivo invece che come uno spintone.

| | velocità peggiore della telecamera |
|---|---|
| tempo fisso a 0,15 s (com'era) | **9,47 nodi al secondo** |
| tempo = distanza a velocità limitata | **2,50** |

Il secondo numero è il sabotaggio della correzione, rimesso il tempo fisso.

**Costo dichiarato:** dare più tempo alla telecamera vuol dire lasciarla
indietro rispetto al corpo più a lungo — fino a 0,6 s invece di 0,15. Quindi
dove sta mentre recupera viene controllato invece che dato per buono:
**0 fotogrammi con la telecamera dentro un blocco** su tutta la spazzata (con la
posizione della telecamera sabotata a tre volte lo scostamento: 230).

---

## 13. Un portale in un verso solo

Chiesto dal gioco: **«fai in modo che il portale lavori solo in una direzione, rendilo
invisibile dalla direzione opposta»**.

### 13.1 Metà c'era già, e nessuno l'aveva misurata

Attraversare da dietro **non è mai stato possibile**. `portal_step_trigger`
mette `entered_from_front` dalla profondità nel fotogramma in cui il giocatore
entra nei limiti del portale: chi arriva dal retro ha profondità negativa, non
arma il grilletto, e per quanto cammini dentro il buco non attraversa niente.

Solo che l'immagine diceva il contrario. `draw_opening_from` costruiva la
geometria dell'apertura con un `side_sign` che la gira verso chi guarda, quindi
da dietro il portale **mostrava la destinazione lo stesso**: una cosa che si
vede attraverso e non si attraversa — le due metà dello stesso oggetto in
disaccordo su come funziona.

Nessun gate lo diceva perché nessun gate aveva mai confrontato le due facce.

### 13.2 Cosa fa adesso

`one_way` in `config/portal.cfg`, settima impostazione, di default **1**. Da
dietro:

- l'apertura non viene disegnata — né il quad dell'immagine né i fianchi del vano
- **il passaggio di rendering per quel link viene saltato**, cioè l'intero
  passaggio del mondo dentro il target
- le diagnostiche di quel link vengono azzerate invece di restare all'ultima
  risposta di quando la telecamera era ancora davanti

Resta il buco nel muro in cui il portale è ricavato e quello che sta oltre il
muro. Nelle scene spike la lastra è spessa un nodo e dall'altra parte c'è campo
aperto, quindi si vede campo: corretto, e meno interessante di un'immagine.
Detto qui invece che lasciato scoprire.

### 13.3 Misurato, nelle due direzioni

`dual_session` sezione 10, in pixel che l'apertura dipinge contro lo stesso
fotogramma senza portale:

| | dal davanti | da dietro |
|---|---|---|
| `one_way 1` | **169246** | **0** |
| `one_way 0` | 169246 | 168216 |

E camminandoci dentro: dal davanti attraversa, da dietro no — con
l'impostazione in tutte e due le posizioni. La riga `one_way 0` è quella che
rende lo zero una misura invece che una telecamera puntata sul niente.

### 13.4 Il regalo: la ricorsione diventa lineare

Non è una potatura, è un cambio di forma dell'albero. Con due aperture che si
guardano, **ogni telecamera virtuale sta dietro l'apertura da cui sta
guardando fuori** — è la destinazione, la si guarda dal retro. Quindi ogni
livello genera **un figlio invece di due**.

| profondità | 0 | 1 | 2 |
|---|---|---|---|
| a due facce | 2 | 6 | 14 |
| in un verso solo | 2 | **4** | **6** |

Esponenziale contro lineare. `portal_recursion` misura tutte e due le colonne:
quella a due facce resta perché è il macchinario che il gate era stato costruito
per controllare, e serve a dire che il risparmio viene dalle aperture guardate
da dietro e non dalla ricorsione che si ferma prima di nascosto.

### 13.5 E da dietro non costa niente

Saltare il passaggio di rendering vuol dire saltare **l'intero passaggio del
mondo** per quel link, che è la cosa più cara del fotogramma. Misurato da tre
nodi dietro l'apertura, guardandone il retro (`--dual-bench`):

| | mondo 512 | mondo 128 |
|---|---|---|
| in un verso solo, portale acceso | 0,339 | 0,189 |
| lo stesso fotogramma senza portale | 0,344 | 0,186 |
| visibile da tutte e due le facce | 0,520 | 0,366 |
| **costo di un portale che non si vede** | **−0,005** | **+0,003** |
| costo di uno che si vede | 0,176 | 0,179 |

Due corse indipendenti, e il segno del primo numero cambia fra le due: è
**zero**, non un risparmio da rivendicare. Dare le spalle a un portale adesso è
come non averlo. Quello che si vede costa 0,18 ms in tutte e due.

Un gate ha dovuto dichiarare la sua configurazione invece di ereditarla:
`portal_cut` guarda un corpo tagliato **da dietro** l'apertura, perché è l'unico
punto da cui la metà rimossa e la copia che la sostituisce sono in campo
insieme. Quella sezione adesso mette `one_way 0` per la sua durata e dice
perché: il taglio e la trasformazione della copia non hanno opinioni su da che
lato del portale stai.

### 13.6 E il difetto che ha scoperto: la profondità non veniva pulita

Dal gioco, subito dopo: **«se faccio il giro del muro che contiene il portale
verticale, mi glitcha tutta la scena»**. Riprodotto in un fotogramma fisso —
`bench/06_behind.png`, «far side of portal A» — e guardato: terreno ridotto a
schegge, metà del mondo mancante, cielo dappertutto. Lo stesso identico
fotogramma con `one_way 0` è perfetto.

Bisezione, quattro esecuzioni, ogni volta lo stesso fotogramma:

| | risultato |
|---|---|
| salto del target **e** dell'apertura | rotto |
| salto della sola apertura, target disegnato | **integro** |
| salto del target, con `gfx_rt_begin`/`clear`/`gfx_rt_end` | rotto |
| salto del target, con lo stato grafico rimesso a default | **integro** |

Quindi non era il portale: era **stato grafico**. E la riga colpevole è più
vecchia di tutto questo lavoro.

**Una `glClear` obbedisce alla maschera di profondità.** Con le scritture di
profondità disabilitate, la pulizia del depth buffer **non fa niente e non dice
niente**. L'HUD spegne la profondità per il suo testo e la lascia spenta a fine
fotogramma; quindi la `gfx_clear` del fotogramma dopo cadeva su un buffer che
non si poteva scrivere. Era invisibile perché **il passaggio del portale girava
per primo e rimetteva lo stato di default sulla sua strada** — riaccendendo le
scritture prima della pulizia principale. Togli quel passaggio, come fa un
portale a senso unico guardato da dietro, e il fotogramma eredita la profondità
di quello prima: la geometria fallisce il test contro una scena che non c'è più.

Corretto dove sta il difetto, in `gfx_clear`, non nel portale: una pulizia che
silenziosamente non pulisce è una trappola per ogni chiamante futuro, e ce
n'erano già dieci.

`dual_session` sezione 11 lo verifica dove morde, non alla chiamata GL: **la
stessa vista disegnata due volte deve tornare la stessa immagine**, qualunque
cosa sia stato il fotogramma prima — e qui il fotogramma prima è uno con l'HUD
acceso, che lascia le scritture di profondità spente.

| | pixel diversi su 2073600 |
|---|---|
| con la maschera riaccesa prima della pulizia | **0** |
| con la riga sabotata via | **1467482** (il 71% dello schermo) |

**Nota su come è stato trovato.** Il portale a senso unico non ha causato questo
difetto, l'ha **scoperto**: era lì da quando esiste l'HUD, mascherato da un
passaggio che girava sempre. È l'argomento migliore che ho per il costo di un
gate mancante — nessuno aveva mai disegnato un fotogramma senza il passaggio del
portale, quindi nessuno l'aveva mai potuto vedere.
