# Tubi

*[Read in English](PIPES.md)*

Guida completa al mod `pipes` (`mods/pipes/`): tubi che portano oggetti da un contenitore
all'altro e reti instradate che li smistano, li conservano e li consegnano su richiesta. Il
modello è il modulo Transportation del classico mod tecnologico
([projectredwiki.com/wiki/Transportation](https://projectredwiki.com/wiki/Transportation)),
ridotto all'essenziale.

La prima parte è per chi gioca, la seconda ([Come funziona dentro](#come-funziona-dentro)) per chi
vuole capire esattamente cosa succede o scrivere un mod che ci parla.

---

## In breve

- I **tubi** si collegano da soli ai tubi sui sei lati. Gli oggetti ci viaggiano dentro come un
  cubetto luminoso.
- Il **tubo per oggetti** è semplice: una pila con una destinazione lo attraversa per la sua
  strada; una pila persa, dove si incrociano tre o più tubi, sceglie a caso.
- Il **tubo di giunzione** è intelligente: manda ogni pila per la strada più corta verso dove deve
  andare.
- Il **tubo d'interfaccia** si attacca a un contenitore (forziere, fornace…) e ospita fino a quattro
  **chip**, che decidono cosa esce e cosa entra.
- Il **tubo di richiesta** mostra in una griglia di icone cosa ha la rete e cosa sa fabbricare, e
  ti manda quello che chiedi.
- Il **tavolo da lavoro automatico** e la **fornace automatica**, blocchi di livello 2 accanto ai
  tubi, fabbricano e cuociono per la rete, anche in catena.
- La **chiave inglese per tubi** gira un'interfaccia o un tubo di richiesta verso il contenitore
  successivo, come il cacciavite.
- Il **tubo firewall** non lascia passare gli oggetti che ha dentro.
- Il **tubo di latenza** rallenta e le strade lo evitano; il **tubo valvola** fa passare solo
  quando il cinabro lo alimenta.
- Il **tubo di crafting** (e i suoi **tubi d'estensione**) dà a una macchina le ricette scelte da
  un pannello, senza bisogno di un esemplare.
- Il **router per tubi** mostra com'è fatta una rete.
- Più chip uguali in una casella danno all'interfaccia una **priorità** più alta.
- Il colore del cubetto dice dove sta andando: **blu** a un ricevitore, **viola** a un terminatore,
  **arancio** al troppopieno, **bianco** su richiesta, **verde** perso.

---

## I tubi

Ogni tubo è un'asta centrale con un braccio verso ogni tubo vicino (e, per l'interfaccia e la
richiesta, verso il loro contenitore). La forma si aggiorna da sola quando metti o togli un tubo o
un contenitore accanto. Rompendo un tubo, qualunque forma abbia, si riprende sempre lo stesso
oggetto. Si rompono a mano in 0,3 secondi.

### Sono solidi

I tubi fermano i corpi: un giocatore o una creatura ci sbatte contro invece di attraversarli. Il
tubo è alto 0,625 di blocco (l'asta centrale va da 0,375 a 0,625), più dei 0,55 che si superano
camminando, quindi **un tubo a terra si scavalca saltando**. Ci si può stare in piedi sopra.

Solo le scatole del tubo sono solide: il resto della cella è aria, ci si passa accanto e la
**luce passa** (nel motore è opaco solo un blocco cubo intero). Un oggetto lasciato cadere su un
tubo ci si posa sopra; il mirino e il contorno seguono la forma del tubo.

### Tubo per oggetti

Nessuna logica. Porta le pile da un tubo al successivo e non tocca mai un contenitore. Una pila
che sta andando da qualche parte **segue la sua strada** anche attraverso gli incroci di tubi per
oggetti: la strada più corta è calcolata tutta in partenza. Solo una pila **persa** (verde),
dove si incontrano tre o più tubi, prosegue in una direzione qualsiasi tranne quella da cui è
arrivata, a caso; in un vicolo cieco torna indietro.

Va bene ovunque. Una giunzione serve dove una pila deve poter **cambiare idea**: ritrovare la
strada se quella vecchia si interrompe, o cercarsi una destinazione se non ne ha.

### Tubo di giunzione

Il tubo instradato. Una pila che lo raggiunge:

1. se ha già una destinazione ancora raggiungibile, riparte per la strada più corta verso di essa;
2. altrimenti se ne cerca una nuova (vedi [Dove va una pila](#dove-va-una-pila));
3. se non ne trova, diventa **verde** e vaga.

È il punto in cui le pile perse ritrovano una destinazione.

### Tubo d'interfaccia

Una giunzione collegata a **un** contenitore accanto: il primo trovato tra i sei lati, nell'ordine
+x, −x, +y, −y, +z, −z. Un **clic destro col cacciavite o con la chiave inglese per tubi** passa al lato successivo che ha un
contenitore; il braccio del tubo si gira verso quello scelto. Se il contenitore viene tolto, il
tubo passa da solo al primo altro che trova.

Un clic destro (senza un tubo in mano) apre i suoi **quattro righe da nove caselle**:

| casella 1 | caselle 2–9 |
|---|---|
| il **chip** | il suo **filtro**: fino a otto oggetti, uno per casella |

Le quattro righe funzionano tutte insieme e indipendenti. Una riga senza chip nella prima casella
non fa nulla. Nel filtro conta solo *quale* oggetto c'è, non quanti.

Rompendo l'interfaccia, chip e filtri cadono a terra. Le sue caselle sono di tipo "materia prima",
quindi nessun estrattore o tramoggia di altri mod porta via i chip.

Un contenitore accanto non deve essere un tubo: l'interfaccia e il firewall hanno caselle proprie,
ma non contano come contenitore di un'altra interfaccia.

### Tubo di richiesta

Un clic destro apre il pannello **Tubo di richiesta**. Mostra tutto ciò che gli **annunciatori**
della sua rete offrono, con il totale di ogni oggetto sommato su tutti i contenitori, e tutto ciò
che la rete **può fabbricare** con quello che ha.

```
┌──────────────────────────────────────────────┐
│ Tubo di richiesta: 36 tipi, 391 in tutto     │
│ ┌──┐┌──┐┌──┐┌──┐┌──┐┌──┐┌──┐┌──┐┌──┐         │
│ │64││64││48││20││ 4││ 1││ 1││  ││  │  ← 9 × 4 │
│ └──┘└──┘└──┘└──┘└──┘└─*┘└──┘└─*┘└─*┘         │
│  …                                           │
│ [<] 1 / 4 [>]  [Per quantità]  [Tutti]       │
│ Cerca: [________________________] [Svuota]   │
│ Chiedi: 8      [1] [8] [Pila] [____]         │
└──────────────────────────────────────────────┘
```

`*` = la chiave inglese: si può fabbricare.

- **La griglia**, in alto: **9 × 4** icone come quelle della borsa. Il **nome** compare passandoci
  sopra col puntatore. Il numero in alto a sinistra è **quanti ce ne sono** nel magazzino. Una
  piccola **chiave inglese in basso a destra** vuol dire che la rete **sa fabbricarlo** con ciò che
  ha e con le sue [macchine](#le-macchine): senza macchine non ce ne sono. Un oggetto che c'è solo da fabbricare non ha numero. **Un clic** sull'icona (o sulla chiave)
  lo chiede.
- **‹ / ›** e il numero di pagina scorrono la griglia, 36 per volta.
- **Per nome / Per quantità** cambia l'ordine (per quantità: prima ciò che hai di più).
- **Tutti / In magazzino / Fabbricabili** sceglie cosa mostrare.
- **Cerca**: scrivi una parte del nome o dell'id (`sand`, `ore:`, `planks`…) e premi Invio; non
  conta maiuscole e minuscole. **Svuota** la toglie.
- **Chiedi**: quanto chiede un clic. **1**, **8**, **Pila** (una pila intera di quell'oggetto: 64,
  16 o 1 a seconda di cosa è), o un numero scritto nella casella e confermato con Invio (fino a
  1024).

Ordine e quantità si ricordano anche passando a un altro tubo di richiesta. Il pannello si chiude
con Esc, o da solo se ci si allontana più di 8 blocchi o il tubo viene rotto.

**Cosa succede a un clic.**

1. Si prende quanto chiesto dagli annunciatori: prima dal più vicino, poi dal successivo.
2. Se nel magazzino non ce n'è abbastanza, quello che manca **viene fabbricato dalle
   [macchine](#le-macchine)** della rete, anche in catena (dai tronchi alle assi ai bastoni). Una
   ricetta che ne fa più di uno alla volta può mandarne qualcuno in più del chiesto. Se non si
   riesce a fare tutto, si fa il massimo possibile.

Tutto arriva come cubetto **bianco**:

- nel contenitore accanto al tubo, se ce n'è uno (scelto come per l'interfaccia, anche col
  cacciavite o la chiave inglese);
- altrimenti cade **sul tubo** stesso, da raccogliere.

Se il contenitore è pieno, quello che non entra riparte come una pila qualsiasi verso un
ricevitore, un terminatore o il troppopieno. Il messaggio dice quanti ne sono partiti (**«N in
arrivo»**, **«N in arrivo, M in lavorazione»**) o **«Niente da mandare»**. Ciò che va fabbricato
arriva quando le macchine hanno finito.

### Tubo firewall

Un clic destro apre **nove caselle**: gli oggetti messi lì **non possono entrare** nel firewall. Gli
altri ci passano come in una giunzione. Le strade si calcolano tenendone conto: una pila di un
oggetto bloccato fa il giro da un'altra parte, se c'è; se non c'è, per lei quella parte di rete non
esiste. Vale anche per le richieste e per gli annunciatori. Rompendolo, il contenuto cade a terra.

---

### Tubo di latenza

Un tubo per oggetti **lento**: una pila lo attraversa a un terzo della sua velocità, e nel
calcolo delle strade vale **8 celle**. Le strade lo evitano quando c'è un'alternativa non troppo
più lunga: utile per spingere il traffico su un altro ramo, o per rallentare apposta un tratto.

### Tubo valvola

Un tubo che si **apre con il cinabro**: aperto mentre accanto a lui qualcosa di `mods/cinnabar` dà
potenza (una leva accesa, un pulsante premuto, una torcia accesa, polvere accesa, un ripetitore o
un comparatore acceso, un blocco di cinabro), chiuso altrimenti.

- **Chiuso**, non passa niente, e le strade lo trattano come un muro: si cerca un giro, e un
  estrattore non manda ciò che solo da lì potrebbe passare.
- Una pila **già in viaggio** la cui strada lo attraversa **si ferma davanti** e aspetta che si apra.
- **Aperto**, è un tubo come un altro.

Con un temporizzatore di cinabro diventa una chiusa a tempo; con un comparatore che legge un
forziere, un rubinetto che si chiude quando il forziere è pieno.

---

## Le macchine

Una rete fabbrica **solo con le sue macchine**, due blocchi di **livello 2**: costano diamanti,
tubi d'interfaccia e un chip, e si scavano solo con un piccone di ferro o migliore. Si mettono
**accanto a un tubo** qualsiasi della rete: il tubo allunga un braccio fino alla porta d'ottone
della macchina.

| macchina | cosa fa | quanto ci mette |
|---|---|---|
| **Tavolo da lavoro automatico** | le ricette del banco da lavoro | 1 s a ricetta |
| **Fornace automatica** | ciò che cuoce una fornace, bruciando combustibile della rete (un carbone ogni 8 cotture, come la fornace) | 2 s a oggetto |

**I chip ricetta.** Una macchina **non sa fare niente da sola**: fa solo ciò che dicono i **chip
ricetta** accanto a lei.

- **Dove vanno:** in una riga di un **tubo d'interfaccia che tocca la macchina**. Gli oggetti nel
  filtro di quella riga sono le sue ricette: per il tavolo ciò che deve fabbricare, per la fornace
  ciò che deve ottenere cuocendo (lingotti di ferro, vetro…).
- **Quante ricette:** fino a 8 per riga e 4 righe per interfaccia, e una macchina può avere più
  interfacce intorno.
- **Il filtro:** per ogni ricetta ci vuole un esemplare dell'oggetto, come per ogni filtro.
- **Nelle catene:** **ogni passo deve avere il suo chip**. Per un piccone di ferro fatto dai
  tronchi servono assi, bastoni e piccone sul tavolo, e lingotti di ferro sulla fornace. Senza le
  assi nel chip, i bastoni non si possono fare, e nemmeno il piccone.

L'interfaccia con i chip ricetta di solito è anche il tubo che porta gli ingredienti alla macchina.

**Tubo di crafting e d'estensione.** Al posto di un'interfaccia con i chip ricetta, accanto alla
macchina si può mettere un **tubo di crafting**:

- **Le ricette:** il clic destro apre il suo pannello, con in alto le sue **9 ricette** (un clic ne
  toglie una) e sotto la ricerca e la griglia di ciò che la macchina sa fare (un clic aggiunge).
  **Non serve un esemplare dell'oggetto**: si sceglie dall'elenco.
- **Più ricette:** ogni **tubo d'estensione** attaccato al tubo di crafting, o a un'estensione già
  attaccata, ne aggiunge altre 9, fino a 16 estensioni. Il clic destro su un'estensione apre lo
  stesso pannello, con le sue.
- **Come ogni tubo**, il tubo di crafting porta anche gli ingredienti.

**Ricorsive.** Quello che chiedi viene pianificato fino a ciò che il magazzino ha, attraverso fino
a **6 ricette in catena**. Per un piccone di ferro, con un tronco, tre ferri grezzi e un carbone in
magazzino, il piano è:

1. la fornace cuoce **3 lingotti di ferro**, bruciando il carbone;
2. il tavolo fa **4 assi** dal tronco;
3. poi **4 bastoni** da 2 assi;
4. poi il **piccone** da 3 lingotti e 2 bastoni.

**Lavorano davvero.**

- **Ingredienti:** partono dagli annunciatori come cubetti **bianchi** e viaggiano nei tubi fino
  alla macchina che li usa.
- **Lavorazione:** mentre lavora la macchina si **accende**: la spia del tavolo diventa verde, la
  fornace mostra il fuoco e la spia ambra.
- **Prodotti intermedi:** vanno alla macchina del passo dopo (i lingotti dalla fornace al tavolo).
  Il prodotto finale va al tubo di richiesta.
- **Contenuto:** un clic destro su una macchina mostra cosa le è arrivato e non ha ancora usato.

**Avanzi.** Le ricette fanno spesso più del necessario: dal tronco vengono 4 assi, ma ne servono 2.
Quando una macchina resta ferma per 5 secondi, ciò che le è rimasto torna nella rete: a un
ricevitore, un terminatore o un troppopieno che lo prende, altrimenti nel magazzino
dell'annunciatore più vicino.

**Più macchine.** Ogni passo va alla macchina più vicina al tubo di richiesta che ha il chip
ricetta per quel prodotto, tenendo conto di quanto lavoro ha già in coda. Ogni macchina fa un passo alla volta, il primo della coda
che ha già tutto. Due macchine dello stesso tipo dividono il lavoro di più richieste.

**Quando non va.** Se un passo che non aspetta più nulla da altri passi resta senza i suoi
ingredienti per **90 secondi**, con la macchina ferma, il lavoro viene abbandonato e chi l'ha
chiesto viene avvisato. Lo stesso succede se una macchina viene tolta. Ciò che era già arrivato
alle macchine torna nella rete come avanzo.

---

## I chip

Si mettono nella prima casella di una riga di un tubo d'interfaccia. Si impilano fino a 16.

**Priorità.** Quanti chip uguali ci sono nella casella è la **priorità** di quell'interfaccia per
quel chip. Tra due ricevitori (o terminatori, o troppopieni) che accettano un oggetto, vince quello
con la priorità più alta, anche se più lontano; a parità, il più vicino. Lo stesso vale per gli
annunciatori: si prende prima da quello con più chip.

| chip | colore | cosa fa | usa il filtro? |
|---|---|---|---|
| **Estrattore** | — | una volta al secondo manda fuori fino a **8** del primo oggetto del contenitore che ha un posto dove andare | sì: solo quegli oggetti; vuoto = tutto |
| **Ricevitore** | blu, veloce | accetta gli oggetti del suo filtro | sì: vuoto = niente |
| **Ricevitore dinamico** | blu, veloce | accetta gli oggetti che il contenitore **ha già** | no |
| **Terminatore** | viola, lento | accetta gli oggetti del filtro, ma solo quando **nessun ricevitore** ha posto | sì: vuoto = niente |
| **Troppopieno** | arancio, il più lento | accetta **qualsiasi cosa** che nessun altro prende | no |
| **Annunciatore** | — | offre il contenuto del contenitore ai tubi di richiesta e ai magazzinieri | no |
| **Magazziniere** | bianco (in arrivo) | tiene nel contenitore **una pila piena** di ogni oggetto del filtro, chiedendola agli annunciatori | sì: è la lista |
| **Ricetta** | — | su un'interfaccia che tocca una [macchina](#le-macchine): ciò che quella macchina sa fare | sì: sono le ricette |

Il **chip vuoto** non fa nulla: è l'ingrediente di tutti gli altri.

### Estrattore

Ogni secondo guarda le caselle del contenitore in ordine (fino a 54) e, per il primo oggetto che
passa il filtro **e ha una destinazione** sulla rete, prende fino a 8 pezzi e li manda. Un oggetto
che non ha dove andare viene saltato, quindi un forziere misto non si blocca su un oggetto che
nessuno vuole. Non manda mai al proprio contenitore.

Prende solo dalle caselle "magazzino" e "prodotto": da un forziere tutto, da una fornace solo il
prodotto finito (non il combustibile né quello che sta cuocendo). Più righe estrattore sulla stessa
interfaccia mandano più pile al secondo.

### Ricevitori, terminatore, troppopieno

Sono le tre **classi** di destinazione, in ordine di priorità: 1 ricevitore (anche dinamico),
2 terminatore, 3 troppopieno. Un'interfaccia accetta solo se il suo contenitore **non è pieno**;
se ha più chip, vale la classe migliore tra quelli che accettano l'oggetto.

Il **ricevitore dinamico** guarda tutto il contenitore: una volta che c'è un pezzo di un oggetto,
gli arriva il resto. Comodo per un magazzino che si organizza da solo: metti a mano il primo pezzo.

### Annunciatore

Rende il contenitore un magazzino visibile alla rete. Offre ciò che sta nelle caselle "magazzino"
e "prodotto" (da una fornace, il prodotto finito). Più annunciatori: si prende prima dal più
vicino al richiedente.

### Magazziniere

Una volta al secondo, per ogni oggetto del filtro: se nel contenitore ce n'è meno di una pila
piena (contando anche quello già in arrivo), chiede la differenza agli annunciatori. Esempio: con
la sabbia nel filtro, il forziere avrà sempre 64 di sabbia finché la rete ne ha.

---

## Il router per tubi

Un attrezzo (vetro, cinabro, oro, un chip vuoto e ferro). Il **clic destro** con il router su un
tubo o su una macchina apre una pagina sulla sua rete:

- quanti tubi, nodi e collegamenti ha, e di che tipo sono i tubi;
- i chip montati nelle interfacce;
- le valvole aperte e chiuse e i firewall;
- ogni macchina, con quante ricette conosce e quanti passi ha da fare;
- quanti oggetti e quante pile sono in viaggio.

---

## Dove va una pila

Quando una pila cerca una destinazione (all'uscita da un estrattore, o a una giunzione se non ne
ha), si guardano le interfacce della rete **dalla più vicina alla più lontana** (le strade più
corte da lì sono già calcolate, vedi [Il grafo](#il-grafo)). Vince:

1. il **ricevitore più vicino** che accetta l'oggetto;
2. se non ce ne sono, il **terminatore più vicino**;
3. se non ce ne sono, il **troppopieno più vicino**;
4. se no, nessuna: la pila diventa **verde** e vaga.

"Vicino" è in tubi percorsi, non in linea d'aria, e i firewall che bloccano l'oggetto sono muri.

La pila segue la strada trovata. Se all'arrivo il contenitore non ha più posto (un altro l'ha
riempito nel frattempo), quel che resta **cerca di nuovo da lì**, escludendo le interfacce che
l'hanno già rifiutata. Se la strada si interrompe (un tubo tolto, un firewall nuovo), la pila
prosegue come può e alla prossima giunzione ricalcola la strada verso la stessa destinazione.

Una pila che arriva al bordo di una zona del mondo non caricata **aspetta lì** finché la zona non
torna in memoria, invece di scambiare l'aria per la fine del tubo e cadere.

### Pile perse

Una pila verde vaga a caso, ma a ogni tubo instradato riprova a cercare una destinazione: appena
qualcuno fa posto, ci va. Una pila in viaggio da più di **un minuto** che si ritrova senza strada
**cade** dove si trova (sul tubo). Una pila il cui tubo davanti viene rotto cade nella cella da cui
veniva.

### Velocità

In blocchi al secondo: ricevitore **6**, richiesta **4**, terminatore **2,5**, perso **2**,
troppopieno **1,2**. Il colore e la velocità cambiano quando cambia la destinazione.

---

## Ricette

Cinabro al posto della pietra rossa dell'originale, coloranti al posto dell'illumar colorato.

| oggetto | quanti | ricetta (righe della griglia 3×3) |
|---|---|---|
| Tubo per oggetti | 16 | pietra · vetro · pietra |
| Tubo di giunzione | 16 | colorante rosso · cinabro · colorante verde / diamante · vetro · diamante / colorante verde · cinabro · colorante rosso |
| Tubo d'interfaccia | 1 | cinabro · oro · cinabro / oro · tubo di giunzione · oro / cinabro · oro · cinabro |
| Tubo di richiesta | 1 | cinabro · tubo di giunzione · diamante |
| Tubo firewall | 1 | pietrisco · tubo di giunzione · cinabro |
| Chip vuoto | 1 | lingotto d'oro · carta · cinabro |
| Tavolo da lavoro automatico | 1 | diamante · chip magazziniere · diamante / tubo d'interfaccia · banco da lavoro · tubo d'interfaccia / ferro · oro · ferro |
| Fornace automatica | 1 | diamante · chip estrattore · diamante / tubo d'interfaccia · fornace · tubo d'interfaccia / ferro · cinabro · ferro |
| Tubo di latenza | 8 | otto tubi per oggetti intorno a sabbia |
| Tubo valvola | 1 | — · cinabro · — / ferro · tubo per oggetti · ferro |
| Tubo di crafting | 1 | cinabro · banco da lavoro · cinabro / oro · tubo di giunzione · oro / cinabro · oro · cinabro |
| Tubo d'estensione | 1 | carta · tubo di giunzione · carta |
| Router per tubi | 1 | cinabro · vetro · cinabro / oro · chip vuoto · oro / — · ferro · — |
| Chiave inglese per tubi | 1 | ferro · — · ferro / — · ferro · — / — · ferro · — (lingotti di ferro) |

Ogni altro chip: **colorante · cinabro · colorante / X · chip vuoto · X / colorante · cinabro ·
colorante**, con:

| chip | colorante | X |
|---|---|---|
| Ricevitore | arancio | lingotto di ferro |
| Ricevitore dinamico | ciano | colorante arancio |
| Troppopieno | verde | lingotto di ferro |
| Terminatore | viola | colorante grigio |
| Estrattore | ciano | lingotto di ferro |
| Annunciatore | magenta | lingotto d'oro |
| Magazziniere | blu | diamante |
| Ricetta | giallo | banco da lavoro |

---

## Schemi pronti

**Svuotare un forziere in un altro.** Forziere A — interfaccia con *estrattore* — tubi — interfaccia
con *troppopieno* — forziere B.

**Smistare.** Un forziere d'ingresso con un *estrattore*; lungo il tubo, un forziere per ogni
materiale con un *ricevitore* che ha quel materiale nel filtro; in fondo un *troppopieno* per il
resto. Giunzioni a ogni incrocio.

**Magazzino con richieste.** Più forzieri, ognuno con un'interfaccia *annunciatore* (e un
*ricevitore dinamico*, così ciò che entra torna dove c'è già). Un *tubo di richiesta* accanto al
forziere da cui lavori.

**Rifornimento automatico.** Accanto alla fornace o al banco, un forziere con un'interfaccia
*magazziniere* filtrata su carbone, sabbia… La rete lo tiene pieno dagli annunciatori.

**Fornace automatica.** Un'interfaccia con *estrattore* sulla fornace porta via solo il prodotto
finito e lo manda ai ricevitori.

### La demo sulla piazza

Sulla piazza, a est dello spawn (x 65–77, z 45–67), ci sono tre reti funzionanti con i cartelli che
spiegano ogni pezzo:

- **Smistare con i tubi**: pietrisco a un ricevitore (blu), costretto a fare il giro da un firewall
  che lo nomina; terra a un terminatore (viola); assi, che nessuno chiede, al troppopieno (arancio).
- **Richieste con i tubi**: un forziere con annunciatore, un tubo di richiesta, un magazziniere
  che tiene una pila di sabbia.
- **Fabbricare con le macchine** (z 60–67): un magazzino di tronchi, ferro grezzo, carbone,
  pietrisco e sabbia; un tavolo da lavoro automatico con un chip ricetta per assi, bastoni,
  piccone di ferro e banco da lavoro; una fornace automatica con un chip ricetta per lingotti di
  ferro, vetro, pietra e carbonella. Chiedi un piccone di ferro al tubo di richiesta: la fornace
  cuoce i lingotti mentre il tavolo fa assi e bastoni, poi il piccone arriva nel forziere accanto
  al tubo. Le macchine guardano verso lo spawn, così si vedono accendersi.

Ogni 15 secondi i rifornimenti si ricaricano e i forzieri finali si svuotano. Una piastra a
pressione ricostruisce tutto com'era.

---

## Limiti

- **Le pile in viaggio sopravvivono a un riavvio**: sono salvate ogni 3 secondi e rimesse nei tubi
  dove erano. Ciò che è stato salvato prima dell'ultima pausa di 3 secondi è al sicuro; quello che
  in quei secondi è partito da un contenitore era ancora nel contenitore al salvataggio del mondo.
- **I lavori delle macchine non riprendono** dopo un riavvio, ma **non si perde niente**: ciò che le
  macchine avevano già ricevuto torna al magazzino appena si ritrovano ferme, e i prodotti già in
  viaggio arrivano. La richiesta va rifatta.
- Una rete ha al massimo **65536 tubi** nel suo grafo: in una più grande, ciò che è oltre non si
  trova.
- Una rete lavora dove il mondo è caricato. Dove ci sono pile in viaggio o macchine con lavoro da
  fare, il mondo **resta caricato** finché non hanno finito, anche senza giocatori vicini. Una rete
  ferma lontano da tutti però non si sveglia da sola: i chip sono un ABM, come ogni macchina del
  gioco.
- Si fabbrica solo con le macchine, solo con ciò che offrono gli annunciatori e fino a 6 ricette in
  catena. Un ingrediente che lascia qualcosa (il secchio di un secchio di latte) lo lascia nella
  macchina, che lo rimanda al magazzino.

---

## Come funziona dentro

### Blocchi e forme

Ognuno dei cinque tipi è registrato in **64 blocchi**, uno per ogni combinazione dei sei bracci:
`pipes:<tipo>` (nessun braccio) e `pipes:<tipo>_<maschera>`, con il bit `d−1` della maschera per la
direzione `d` nell'ordine +x, −x, +y, −y, +z, −z. Le forme sono `core.node_box`: l'asta
`[0,375 … 0,625]³` più un braccio fino al bordo della cella per ogni bit. Tutte le forme
`node_drop`-pano nella forma 0.

I blocchi sono `solid = true`: il motore ferma i corpi sulle scatole e non sulla cella, e un
blocco con scatole non è mai opaco, quindi la luce passa. Le pile in viaggio sono entità ECS del
mod, spostate a mano: la solidità non le riguarda.

`refresh(cella)` ricalcola la maschera (tubi vicini, più il lato del contenitore per interfaccia e
richiesta) e cambia blocco se serve; `core:placed` e `core:broken` lo chiamano sulla cella e sulle
sei vicine. Il lato del contenitore è salvato nel metadato `side` della cella.

### Una pila in viaggio

Una pila (`Stack`) ha l'oggetto, quanti, la cella da cui viene e quella verso cui va, l'avanzamento
`t` tra le due, la velocità, la strada (una lista di celle) o nessuna, la destinazione come chiave
`"x,y,z"`, l'età e le interfacce che l'hanno già rifiutata. A ogni `core:tick` avanza di
`velocità × dt` celle; ogni volta che `t` arriva a 1 entra nella cella successiva (`arrive`), fino a
4 celle per tick.

All'arrivo in una cella:

1. non è più un tubo → la pila cade nella cella precedente;
2. ha più di 60 s e nessuna strada → cade lì;
3. è la destinazione → `deliver`: `container_add` nel contenitore dell'interfaccia (o, per una
   richiesta senza contenitore, `drop_item`); il resto si segna la cella come rifiutata e ricalcola;
4. ha una strada e la cella successiva è ancora percorribile → la segue, anche negli incroci di
   tubi per oggetti;
5. altrimenti: in un tubo instradato `reroute` (prima verso la vecchia destinazione, poi una nuova
   con `route`, poi verde), in un tubo per oggetti `wander`.

### Il grafo

Una rete non viene esplorata cella per cella ogni volta che una pila cerca strada: viene ridotta una
volta sola a un **grafo**.

- **Nodi**: le celle dove si decide qualcosa. Sono tutti i tubi instradati (giunzione,
  interfaccia, richiesta, firewall) e i tubi per oggetti che non stanno a metà di un tratto, cioè
  con tre o più vicini, o uno solo (fine), o nessuno.
- **Archi**: i tratti di tubi per oggetti fra due nodi, con le loro celle e la lunghezza. Una rete
  di mille tubi diventa così qualche decina di nodi.

`build` lo costruisce con una visita in ampiezza (fino a `MAX_NET` = 65536 celle).
`ways_of(rete, nodo, oggetto)` fa **Dijkstra** da un nodo sul grafo, con il peso di un arco pari
alla sua lunghezza in celle. Restituisce distanza, arco e nodo precedente di ogni nodo, e le
interfacce in ordine di distanza. Il risultato **resta in cache** nella rete, una volta per ogni
combinazione di firewall che fermano l'oggetto (di solito nessuna, quindi una sola per nodo di
partenza; al massimo `MAX_ROUTES` = 4096, poi si ricomincia). `way` ricostruisce dalla cache la
lista di celle che la pila percorre.

`route` scorre le interfacce in ordine di distanza: si ferma al primo ricevitore e ricorda il primo
terminatore e il primo troppopieno. `class_of` dice la classe di un'interfaccia per un oggetto
(nil se il contenitore è pieno, `container_fill ≥ 1`). La scelta tra interfacce resta dal vivo,
perché chip, filtri e spazio libero cambiano di continuo. Il grafo invece contiene solo ciò che
cambia quando cambiano i tubi.

**Quando si aggiorna.** Ogni cella della rete e ogni cella accanto a una sua cella è osservata con
`core.watch_cell`, che segnala ogni scrittura (di un giocatore, di un mod con `core.set_block`, di
un pistone…) come `core:watched`. Se una cella della rete cambia *tipo* di tubo o smette di essere
un tubo, o se in una cella accanto compare un tubo, il grafo viene **scartato**. Un tubo che cambia
solo forma è lo stesso tubo e non conta. Il grafo nuovo si costruisce alla prossima domanda, non
subito, quindi cento modifiche di fila costano una ricostruzione sola. `core:placed`, `core:broken`
e `pipes:refresh` scartano subito, senza aspettare l'evento. Una zona del mondo ricaricata, che
`watch_cell` non vede, scarta i grafi che la attraversano appena torna in memoria: diciotto LBM
(`pipes:loaded_1` … `_18`, uno ogni 32 delle 576 forme dei nove tubi) chiamano `touched` su ogni tubo
caricato. Un grafo più vecchio di `STALE_AFTER` (30 s) viene ricostruito comunque, come rete di
sicurezza. Le celle osservate non vengono mai lasciate, perché l'osservazione è
un insieme unico per tutti i mod e un osservatore di `mods/cinnabar` potrebbe guardare la stessa
cella.

**Cache dei contenitori.** Le righe di chip di un'interfaccia, gli oggetti di un firewall e ciò che
offre il contenitore di un annunciatore si rileggono solo quando cambia
`core.container_revision` del loro contenitore.

**Quanto conta.** Su una griglia di 30×30 tubi con 20 estrattori su forzieri di oggetti che
nessuno vuole (il caso peggiore: ogni secondo ogni estrattore cerca strada per 20 oggetti), 60
secondi di gioco costavano 28,3 s di ABM con la ricerca cella per cella e costano 0,69 s con il
grafo: da 23,9 a 0,9 ms per tick.

### I chip

Due ABM (`pipes:chips` e `pipes:chips_2`, perché un ABM nomina al massimo 32 blocchi) passano ogni
secondo su ogni interfaccia e per ogni riga eseguono estrattore e magazziniere. Gli altri chip sono
passivi: li legge `class_of` quando qualcuno cerca una destinazione, o `broadcasters` quando qualcuno
chiede.

`broadcasters(verso, oggetto)` prende dalla cache di Dijkstra del richiedente le interfacce con un
annunciatore, dalla più vicina (un contenitore con due annunciatori conta una volta). Per ognuna la
strada, al contrario, si calcola solo se da lì parte davvero qualcosa. `request` prende da ognuna (con
`container_take` su "store" e poi "product") a pile intere finché non ha il numero chiesto, e manda
le pile bianche con la destinazione già fissata. I magazzinieri tengono il conto di quel che hanno
chiesto e non è ancora arrivato in `pending[cella][oggetto]`, che scende alla consegna o quando la
pila cambia destinazione.

### Il pannello di richiesta

Un pannello del motore (`core.ui_define`, `ui_item`, `ui_button`, `ui_field`): le icone con il
tooltip del nome le disegna il client, il mod dice solo cosa c'è e dove. Un pannello ha al massimo
**128 elementi** e il server ne tiene **64** per tutti i mod insieme. Per questo:

- la griglia è **9 × 4** e ogni casella costa fino a tre elementi (icona, numero, chiave inglese):
  108, più 15 di controlli, fa 123;
- i pannelli sono **`PANELS` = 6**, `pipes:request_1` … `_6`, dati a turno a chi apre un tubo di
  richiesta. Quando sono tutti in uso, quello guardato da più tempo viene chiuso al suo giocatore.

Per ogni giocatore una *vista* ricorda il tubo aperto, la ricerca, l'ordine, cosa mostrare, la
pagina, la quantità e quali oggetti sono in quale casella. `draw_request` ridichiara il pannello
dopo ogni clic. I clic arrivano come `core:ui_click`: `i<n>`/`w<n>` (la casella n: icona o chiave
inglese), `prev`, `next`, `order`, `show`, `find` (con il testo), `clear`, `a1`, `a8`, `astack`,
`amount` (con il testo), `core:esc`. Ognuno vale solo se il tubo c'è ancora e il giocatore è entro
8 blocchi; altrimenti il pannello si chiude. Il testo scritto perde i caratteri di controllo e
viene tagliato a 40 byte; la quantità è limitata fra 1 e `MAX_ASK` = 1024.

`stock` conta per ogni oggetto solo gli annunciatori da cui può davvero arrivare: in una rete con
firewall, per ogni oggetto guarda le strade di Dijkstra calcolate per lui (le stesse della
richiesta, in cache), così il pannello mostra solo ciò che una richiesta ottiene.

### Le macchine e i piani

**Le ricette.** Quelle del banco le dà il motore: `core.recipe_count()`, `core.recipe_result(i)`,
`core.recipe_yield(i)` e `core.recipe_items(i)` (ingredienti per nome, una parola per casella,
senza forma). Quelle della fornace le dà `mods/furnace`: all'evento `furnace:ask` risponde subito
con un `furnace:smelt {from, to}` per ogni cottura e un `furnace:fuel {item, smelts}` per ogni
combustibile. `ways_to_make()` le legge **una volta**, alla prima richiesta, e le tiene per id del
prodotto come *modi* `{kind = "craft" | "smelt", makes, yield, needs}`.

**Il piano.** `plan(magazzino, oggetto, q, macchine)` è una ricerca in profondità:

- `need(id, q, passo)` prende dal magazzino quanto può (lo segna in `passo.uses`, che verrà portato
  alla macchina) e il resto lo chiede a `make`;
- `make(id, q, padre)` prova i modi di farlo che le macchine presenti sanno fare, con `⌈q ÷ resa⌉`
  ripetizioni, e per una cottura il primo combustibile che basta;
- ogni prelievo dal magazzino va in un registro e un tentativo fallito viene **annullato** fino al
  suo segno; un oggetto già in lavorazione più su nella catena non viene riprovato (niente cicli);
- limiti: `MAX_DEPTH` = 6 ricette in catena, `PLAN_BUDGET` = 4000 tentativi per piano.

I passi escono in ordine di esecuzione: i figli prima del padre. Ogni passo sa a chi va il suo
prodotto (`parent`) e quanti figli aspetta (`children`).

`makeable` chiede un piano *a vuoto* (`dry`) per uno di ogni prodotto, e lo usa il pannello per la
chiave inglese. Con un magazzino di 638 tipi e le due macchine, 286 oggetti risultano fabbricabili
e un disegno del pannello costa circa 3,5 ms.

Se il piano per tutta la quantità non riesce, `craft` cerca per dimezzamenti la quantità più
grande che si può fare.

**Le ricette di una macchina.** `abilities(rete)` guarda, per ogni macchina della rete, i tubi
d'interfaccia sui suoi sei lati e le righe con il chip ricetta (`rows_of`, in cache fino a che il
contenitore cambia). Restituisce `[tipo][prodotto]` = le macchine che lo sanno fare. `plan` prova
solo i modi il cui prodotto è lì, e `makeable` e `craft` partono da lì: il pannello e i lavori
vedono subito un chip messo o tolto.

**Il lavoro.** `craft` assegna ogni passo, tra le macchine che hanno il suo chip, a quella con punteggio più basso
(distanza dal tubo di richiesta + 16 per ogni passo già in coda). Poi manda a ogni macchina, dagli
annunciatori, ciò che il suo passo prende dal magazzino. I cubetti hanno `into` = la cella della
macchina, e `deliver` li mette nel suo contenitore.

**Il turno di una macchina.** Un gestore di `core:tick` passa sulle macchine con lavoro:

- se ne ha uno in corso e il tempo è scaduto, consegna il prodotto: nel proprio contenitore se il
  padre è sulla stessa macchina, altrimenti spedito alla macchina del padre, o al tubo di richiesta
  per l'ultimo passo;
- altrimenti cerca il primo passo della coda che ha nel contenitore tutto ciò che gli serve
  (`needs`), lo preleva e accende la macchina (cambia blocco tra `_on` e spento, mantenendo
  l'orientamento);
- se un passo senza figli in attesa aspetta da più di `WAIT_FOR` (90 s, contati dall'ultimo arrivo
  alla macchina), il lavoro viene abbandonato;
- una macchina ferma da `IDLE_FLUSH` (5 s) rimanda indietro gli avanzi, una pila per turno.

**Le macchine nel grafo.** Una macchina non è un tubo. `build` la trova tra le celle accanto alla
rete e la registra in `net.machines` con il tubo che la alimenta, e quel tubo diventa un nodo del
grafo. Le macchine non contano come contenitori per interfacce e tubi di richiesta. Il cambio
acceso/spento non scarta il grafo, perché è la stessa macchina.

**Gli avanzi delle ricette.** `ways_to_make` chiede al motore, per ogni ingrediente,
`core.item_leaves(nome)` (ciò che `core.item_returns` ha detto che resta): quando un passo parte, la
macchina mette nel suo contenitore ciò che gli ingredienti spesi lasciano, e lo rimanda al
magazzino quando è ferma.

**Dopo un riavvio.** I lavori non vengono salvati. Un LBM (`pipes:machines`) ritrova le macchine
quando i loro chunk si caricano, le spegne se erano rimaste accese e le registra: ferme e senza
lavoro, rimandano al magazzino ciò che hanno dentro. Le pile che andavano verso una macchina sono
salvate con il loro `into` e ci arrivano, e poi tornano indietro allo stesso modo.

### Le pile attraverso un riavvio

Ogni `SAVE_EVERY` (3 s) `save_transit` scrive nello storage del mod (`core.storage_set`, salvato
nel file del mondo) ogni pila in viaggio: `oggetto|quanti|cella da|cella a|destinazione|into|chiesta`,
separate da `;`, in tanti valori quanti servono (`transit_1`, …, al massimo 4 KB l'uno;
`transit_n` dice quanti) e svuota quelli che non servono più. A `core:world_ready`
`restore_transit` le rimette nei tubi: ripartono dalla cella in cui erano, senza strada ma con la
destinazione, e al primo tubo instradato la ritrovano. I conti dei magazzinieri (`pending`) si
rifanno dalle pile chieste. Una pila la cui cella davanti non è in memoria (`core.loaded`) aspetta.

**I modelli.** `tools/make_pipes_machines.py` (Blender, `--background`) costruisce i quattro
modelli -- `autocrafter`, `autocrafter_on`, `autofurnace`, `autofurnace_on` -- li esporta in
`mods/pipes` e lascia il lavoro in `assets/blender/pipes_machines.blend`. Le icone e i modelli in
mano vengono da `tools/make_held_models.py` (`ONLY=pipes:autocrafter,pipes:autofurnace`).

### Per chi scrive un mod

- **`pipes:refresh {x, y, z}`**: ricalcola la forma di una cella. Da emettere dopo aver messo tubi
  con `core.set_block`, che non solleva `core:placed`. Il grafo della rete si aggiorna da solo
  (`core.watch_cell`): l'evento serve per la forma.
- **`pipes:clear {x0, z0, x1, z1}`**: fa sparire senza farle cadere le pile in viaggio in quel
  rettangolo (qualunque y). Serve a chi ricostruisce una rete (lo usa la demo).
- Un tubo d'interfaccia o firewall messo con `core.set_block` non ha caselle: crearle con
  `core.container_make(x, y, z, 36)` (interfaccia) o `9` (firewall). La demo mostra come riempirle.
- Qualsiasi blocco con un contenitore (`core.container_make`) funziona accanto a un'interfaccia;
  i ruoli delle caselle (`core.container_roles`) decidono da dove estrattore e annunciatore
  prendono.

I numeri da cambiare sono in cima a `init.luau`: `MAX_NET`, `STALE_AFTER`, `MAX_ROUTES`,
`PANELS`, `MAX_ASK`, `LOST_FOR`, `TAKE`, le velocità in `CLASS`, `REQUESTED`, `LOST`, e per le macchine
`MAX_DEPTH`, `CRAFT_SECONDS`, `SMELT_SECONDS`, `WAIT_FOR`, `IDLE_FLUSH`, `PLAN_BUDGET`, `SAVE_EVERY`,
e per i tubi nuovi `LATENCY_COST`, `LATENCY_SLOW`, `RECIPE_SLOTS`, `MAX_EXTENSIONS`, `KEEP_EVERY`.

### I tubi nuovi, dentro

- **Valvole:** sono sempre nodi del grafo (`net.valves`). `ways_of` aggiunge ogni valvola chiusa
  all'insieme dei nodi esclusi e alla chiave della cache, quindi aprire o chiudere una valvola
  sceglie un altro insieme di strade senza buttare il grafo. `valve_open` guarda i sei vicini.
  `arrive` ferma una pila davanti a una valvola chiusa della sua strada.
- **Latenza:** la lunghezza di un arco è la somma delle sue celle, `LATENCY_COST` per un tubo di
  latenza; nel tick la pila va a `LATENCY_SLOW` della sua velocità mentre è in uno.
- **Priorità:** ogni riga di chip ha `prio` = quanti di quel chip ha l'interfaccia (in cache con le
  righe). `class_of` restituisce classe e priorità; `route` guarda tutte le interfacce e tiene per
  ogni classe la priorità più alta, la più vicina a parità; `broadcasters` ordina per priorità e
  poi per distanza.
- **Tubi di crafting:** le ricette sono nei metadati `r1` … `r9` della cella (nomi di oggetti).
  `recipes_of_pipe` le raccoglie con quelle delle estensioni raggiungibili, e `abilities` le dà
  alla macchina che il tubo tocca. Il pannello (`pipes:recipes_1` … `_3`, a turno) offre i prodotti
  dei modi di fare del tipo della macchina servita (`served_kind`).
- **Router:** `show_network` scrive la pagina `pipesnet`.
- **Lavoro lontano:** ogni `KEEP_EVERY` (20 s) `core.keep_loaded` sulle colonne di ogni pila in
  viaggio e di ogni macchina con lavoro, che restano in memoria i 30 s seguenti.
