# L'assalto

L'assalto finale alla dimensione di Basoalth, da giocare insieme. Due mod:
`mods/assault_core` (solo server: la valle, le fortezze, la sala del nucleo,
il tunnel della fuga) e `mods/assault_game` (`side = both`: le fasi, i
crediti e il negozio, la sala, Basoalth, la fuga sul server; HUD ed effetti
sul client). Armi, granate, bot e mezzi sono quelli del fronte
(`games/front`, `front_game`), copiati e voltati al gioco contro il computer.

## Come ci si arriva

`tools/track.sh` (o il menù del titolo) avvia il sandbox con undici stanze;
la porta dell'assalto, un cancello di basalto nero con due braci viola, sta a
90.5 21 76.5 (`mods/gates`, diciassettesima bocca del sandbox: il motore ne
regge venti dal protocollo 107). Come il fronte, nel gioco finito si apre solo
a chi ha battuto l'Ospite nel laboratorio; per ora `FRONT_OPEN = true` in
`mods/keys/init.luau` lascia aperti tutti e due.

Il mondo non si salva (nessun `--world-file`): quando nessuno ci è più stato
per mezzo minuto le sue colonne tornano come le fa il mapgen, e la dimensione
è di nuovo intera. Lo stato della partita si azzera 20 s dopo che l'ultimo
se n'è andato.

**Provare solo la fuga**: `tools/escape.sh` avvia un server col solo mondo
dell'assalto e un client; tre secondi dopo che si è dentro, `tools/assault_escape`
salta fortezze e sala (evento `assault:to_escape`): si è nella sala, il nucleo
si spezza, Basoalth si alza, il pavimento crolla e le jeep aspettano nel tunnel.
Ricomincia ogni volta che qualcuno rientra in un mondo vuoto.

## La valle

128 nodi di larghezza, 1080 verso nord; a est un crepaccio di magma lungo
il quale corre il tunnel della fuga. Tutt'attorno, fino a 110 nodi fuori,
una catena di montagne che sale allontanandosi (fino a 48 sopra il terreno)
e nove vulcani: un cono alto 66-90, un cratere pieno di magma e tre colate
che scendono ondeggiando verso la valle (`VOLCANOES` in `assault_core`).
Un fiume di lava attraversa la valle fra il bastione e la batteria (z 590-
610, ondeggia): scende da una gola nelle montagne a ovest, corre tre blocchi
sotto il terreno fra argini di basalto e cade nel crepaccio a est. Si passa
sul ponte della strada (9 di larghezza, parapetti e lampade) o, ai lati
(x 22 e 104), saltando su tre isolotti di basalto, uno o due blocchi
l'uno dall'altro. La lava (`assault_core:lava`) uccide giocatori e soldati
e distrugge i mezzi; i bot alla guida la evitano. Il cielo è
rosso (`core.world_storm`, protocollo 108): il sole c'è ancora, ma le nuvole
sono del colore del cielo e girano in vortici qua e là: ognuno ruota tutto
intero (un giro ogni mezzo minuto circa) avvolto in due bracci a spirale, con
un occhio sereno al centro. Alcuni minacciano un tornado: una parete scura al
centro e sotto un imbuto che scende verso la valle e risale (fino a 44, mai a
terra), ogni minuto e mezzo circa. Costo: con llvmpipe in Xephyr il campo
passa da 25.7 a 21 FPS.

| fase | dove (x, z) | cos'è | terre attorno | guarnigione (1 giocatore, +2 per ognuno in più) |
|---|---|---|---|---|
| campo | 64, 22 | cortile di cemento, la porta d'arrivo, le casse | prato | -- |
| 1 l'avamposto | 64, 250 | palizzata di tronchi, due torrette, una capanna | palude morta: fango, pozze, canne, alberi secchi | 8, due mitragliatrici |
| 2 il bastione | 64, 480 | mura di pietra, torri, un mastio | foresta bruciata: cenere, tronchi carbonizzati, braci | 11, due mitragliatrici, due mortai, un carro |
| 3 la batteria | 64, 710 | tre bunker, trincee, due obici, contraerea | distesa di cristalli viola e luminosi | 14, due mitragliatrici, un elicottero |
| 4 la fortezza | 64, 905 | mura di basalto alte 12 (indistruttibili), il cancello da far saltare | basalto spaccato dal magma, costole d'osso | 18, due carri, mortai, contraerea, un elicottero |
| la sala | 64, 976 | dietro la fortezza: quattro piloni, il nucleo, il trono | -- | ondate da porte nei muri laterali |

La legione è di tre specie, scelte per come combattono: **zombi** (dietro i
ripari, lenti, 28 di vita), **demoni** (all'assalto, veloci, 18) e
**cyborg** (di guardia col fucile di precisione e col lanciarazzi, 26).

## Come si gioca

* **Una fortezza** si prende stando alla sua bandiera (10 nodi) senza
  nessuno della legione entro 20: la presa sale di 10 al secondo per ogni
  alleato, fino a tre. Presa, la bandiera diventa blu, chi resta della
  guarnigione scappa, la base si sposta lì (si rinasce lì, le casse
  ricaricano) e si arma la successiva. Il cancello della fortezza va fatto
  saltare con granate, razzi o un carro.
* **Crediti**: una cassa sola per tutti. Un nemico ucciso da un giocatore 10,
  da un bot alleato 5, una fortezza 150, un pilone 100; e un rifornimento
  lento, 5 ogni 5 secondi finché si combatte (fortezze e sala).
* **La base** è sul terreno aperto a nord dell'ultima fortezza presa, sulla
  strada per la successiva: lì si rinasce, le casse ricaricano, i soldati
  alleati caduti tornano a ondate ogni 15 s e i mezzi alleati (anche quelli
  comprati) hanno ognuno il suo posto nel parcheggio. Un mezzo alleato senza
  giocatori a bordo, fermo per 40 s lontano dal parcheggio (incastrato,
  in una buca), torna nel parcheggio. L'avamposto e il bastione hanno un
  varco anche a nord, le trincee della batteria uno sulla strada.
* **Negozio** (B entro 30 nodi dalla base): cinque soldati in più (150, fino a
  100), un costruttore (120, fino a 6), mitragliatrice leggera (120), fucile di
  precisione (120) e lanciarazzi (200) a tutti; potenziamenti a livelli, ogni
  livello più caro del precedente: armi +15% di danno (150, x1.6, fino a 8),
  mezzi +25% di resistenza (180, x1.6, fino a 6), corazza antiesplosione -15%
  (140, x1.7, fino a 4); jeep (100), carro (350), elicottero (500). Si parte
  con fucile, pistola, granate, quattro soldati e due costruttori.
* **Zone bonus**: ai lati di ogni fortezza (x 10 e 118) una radura con un
  faro, tenuta da tre della legione; il suo fascio di luce si vede da
  lontano, viola o blu. Presa (stando al faro senza nemici entro 14), +50
  crediti e +3 a ogni rifornimento. Un alleato su tre va a prenderle da sé.
* **Contrattacchi**: ogni 100-140 s, mentre si combatte per una fortezza, ne
  esce una jeep (due con più giocatori, o a volte) piena di soldati nuovi
  che punta alla vostra base, li fa scendere lì e combattono.
* **Costruttori**: alleati disarmati e più resistenti (50 di vita), con la
  chiave inglese: vanno ai crateri dietro la fronte (basi prese, strada) e
  li rimettono com'erano. Due all'inizio, altri nel negozio (fino a 6). Metà
  pensa prima a costruire per la fortezza da prendere: una scala sulla faccia
  sud di ciò che sta a 12 e 22 nodi dal centro (muro, torre o bunker alto
  3-16 con spazio sopra) e quattro nidi di sacchi 25 nodi davanti all'area.
  Un costruttore fermo 3 s fa un passo di lato; un lavoro che non raggiunge
  in 25 s lo lascia per 45 s; il lavoro di uno morto torna libero. Provato:
  all'avamposto due scale e quattro nidi.
* **Caserme**: soldati alleati e costruttori escono da una caserma di tavole
  21 nodi a ovest della base, porta a est. Quella del campo c'è sempre; le
  altre appaiono quando la base si sposta lì (`assault:barracks`), e da
  allora mapgen e riparazioni le contano come parte della dimensione. Aperto:
  non provato che un client entrato dopo veda quelle delle basi prese.
* **Il cancello**: davanti al cancello della fortezza, entro 30 nodi, un
  alleato a piedi alla volta va fino a 7 nodi e fa esplodere una granata
  nella cella di cancello più vicina che resta al centro, ogni 3.5 s; se in
  15 s non ci riesce passa a un altro. Provato: aperto in circa 45 s.
  `ponytail`: la granata è messa nel cancello, non lanciata (lanciata
  rimbalzava indietro, posata ai piedi cadeva nel cratere).
* **Rinascita**: chi muore (colpito, esploso, caduto, nel magma) torna con
  tutte le armi comprate e caricatori, riserve e granate pieni. Le morti date
  dal motore si riconoscono dal salto al punto di rinascita.
* **Tornado**: mentre si combatte per una fortezza, ogni 45-85 s (il primo
  dopo 30) ne scende uno, al massimo due insieme, sul terreno aperto fra la
  base e la fortezza; vaga a 1.5-2.5 m/s per 45-75 s. Entro 14 nodi risucchia
  e fa girare giocatori, soldati di tutte e due le parti e mezzi (i carri
  appena); al centro (3.5) lancia in alto e fuori e toglie 7, ogni 2 s.
  Provato: un giocatore sale di 4.4 nodi, è sbalzato di 10, risucchiato e
  morto al terzo lancio; quattro soldati morti in pochi secondi. Il lancio
  di un giocatore resta sotto i 16 m/s per asse (`net::Push::kMax`). Lo
  disegna il passo delle nuvole (`core.storm_funnel`): un imbuto dal ventre
  delle nuvole a terra, polvere alla base. Costo: con llvmpipe in Xephyr due
  tornado vicini portano il campo a 11 FPS (21 senza).
* **Da passeggero** (sedile senza arma del mezzo) si spara con la propria
  arma e i tasti 1-9 la scelgono; cambiano sedile solo al volante o a
  un'arma del mezzo. All'inizio di ogni tentativo di fuga le armi di tutti
  sono di nuovo piene (sulla strada non ci sono casse).
* **Crediti di prova**: `tools/track.sh --fresh` mette nella stanza anche
  `tools/assault_test`, e ogni partita parte con 100000 crediti.
* **Sui mezzi**: chi non guida e non sta a un'arma del mezzo spara con la
  sua (giocatori e bot), e mira col destro. I bot seduti sono portati dal
  motore (`core.mob_ride`) insieme al mezzo.
* **La sala**: i quattro piloni nutrono lo scudo del nucleo; abbattuti, il
  nucleo resta scoperto 18 s, poi i piloni si riaccendono più forti.
  Basoalth tira sfere dal trono, che un colpo spegne.
* **L'ascesa**: rotto il nucleo, Basoalth ne prende l'energia, cresce fino a
  34 metri e sfonda il tetto; poi il pavimento della sala crolla e tutti
  cadono (senza danno: la vita torna piena prima e dopo) fra cumuli di
  macerie, al fondo del tunnel della fuga.
* **La fuga**: tre jeep al fondo del tunnel, uno guida e gli altri sparano;
  da soli un bot guida o tiene la mitragliatrice. Il tunnel (`assault_core`,
  `TUBE`; `A.PATH` in `assault_game`) è largo 24 e alto 44, perché il
  gigante ci cammina in piedi: a nord sotto il garage, a est sotto il
  crinale, poi giù per il crepaccio in curve a destra e a sinistra (15 curve
  ad angolo retto, piane per 12 nodi attorno a ogni angolo), salite e
  discese fra -2 e 10 (mai più di un blocco ogni 8 nodi), fino alla roccia
  con la porta di uscita, all'aperto. Lampade lungo le pareti. È un
  canale di scolo della lava: la strada è larga 13 al centro, ai lati fino
  alle pareti scorre il magma due blocchi più in basso (le curve sono strada
  da parete a parete, così chi taglia la curva non ci cade); chi ci finisce,
  a piedi o in jeep, è perso. Dopo 14 s il
  gigante si muove e il pavimento crolla dietro di lui (9 m/s, fino a 14,
  +6 se si è più di 50 m avanti); ogni pochi secondi un pugno apre una buca
  su un lato (segnata in rosso 1.6 s prima), e le sue sfere inseguono le
  jeep. Ogni colpo su di lui lo frena (una mitragliatrice che lo tiene sotto
  tiro lo porta a circa tre quarti del passo, due a meno di due terzi, mai
  sotto il 40%; la testa conta doppio) e abbastanza colpi alla testa lo
  fanno barcollare 3.5 s. Presi tutti: si rifà solo il pavimento (in circa
  un secondo), le jeep tornano 2 s dopo e si riprova dal fondo del tunnel.
  Una jeep sulla roccia in fondo, e la porta di casa si apre a tutti.
* **Trappole nel tunnel**: ogni 110 m circa, a rotazione, un obiettivo da
  colpire, rifatto a ogni tentativo (anche i bot a bordo ci sparano, al
  momento giusto: i massi subito, la valvola con lui entro 150 m, il
  serbatoio entro 55):
  la **valvola** del magma su una parete (40 di vita) apre un getto di lava
  attraverso il tunnel per 25 s, e Basoalth che ci passa va al 45% per 5 s;
  il **grappolo di massi** sotto il soffitto (60) crolla nei canali, e lui
  ci inciampa e barcolla 4 s; il **serbatoio di gas** in un canale (50)
  esplode subito, e se lui è entro 40 m lo frena molto e lo fa barcollare
  2.5 s (troppo presto, è sprecato). Per ognuna Basoalth ha la sua
  animazione (`tools/make_assault_models.py -- --giant`): nella lava
  pesta i piedi e si batte le gambe con la testa all'indietro (burn), sui
  massi si piega in avanti fino a terra e si rialza (trip), il serbatoio lo
  sbalza indietro e lui alza le braccia davanti alla faccia (blast). Provato colpendo ognuno a 25 m: con un
  bot alla guida si arriva a circa 900 m invece di 500.
  Provato con un bot alla guida e nessuno che spara: circa 1100 dei 1440 m
  prima di essere presi (500 prima che i gradini avessero rampe lunghe tre
  celle, `core/physics/craft.cpp`).

## I modelli

`tools/make_assault_models.py` (Blender): zombi, demoni e cyborg, Basoalth e
il gigante, il nucleo, i piloni. Il resto è del fronte
(`tools/make_front_models.py`) e del laboratorio (sfere e scudo,
`tools/make_ospite.py`).
