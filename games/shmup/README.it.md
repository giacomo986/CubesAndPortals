# Lo sparatutto da sala giochi

*[Read in English](README.md)*

Uno sparatutto visto dall'alto su un caccia, come nei cabinati degli anni ottanta: uno schermo con
il mare che scorre sotto, dieci ondate di aerei, i power-up, una fortezza volante alla fine, tre
vite, un punteggio e un record. Due mod: `mods/shmup_core` (solo server: il mare, l'isola con
l'hangar, i muri dello schermo, i modelli, e il corpo -- il robot del sandbox, a piedi) e
`mods/shmup` (`side = both`: la partita sul server; il decollo, la telecamera, il paesaggio che
scorre, i colpi e le scritte sulla macchina del giocatore).

## Come ci si arriva

Nel gioco (il menù del titolo, o `tools/track.sh`) il suo cancello è sul lato sud della piazza, a
56.5 21 26.5: un cabinato nero da sala giochi (`mods/gates`). Il mondo non parte col server: sta
nel file delle stanze, si avvia quando qualcuno si avvicina al cancello e si ferma un minuto dopo
che l'ultimo se n'è andato; il cancello è un portale fra stanze che prende in prestito una bocca.
`tools/shmup.sh` avvia il sandbox con questo mondo soltanto, dietro la prima apertura
(`tools/shmup.sh -- --room shmup` ci nasce dentro). In tutti e due i casi si esce da una porta nel
muro di fondo di un hangar, a piedi, guardando i quattro caccia parcheggiati e il mare oltre il
fronte aperto. La porta riporta indietro.

Avvicinati a un caccia e premi **F**: rulla lungo il piazzale, decolla e sale, e la telecamera
gira da dietro l'aereo a sopra il campo. Il primo caccia in volo fa partire la partita; chi sale
a partita iniziata entra in quella.

## La partita

| tasto | |
|---|---|
| F | accanto a un caccia parcheggiato: decolla. In volo: torna all'hangar (fuori da questa partita); lo fa anche E |
| W, A, S, D | muovono il caccia su, a sinistra, giù, a destra: guarda sempre verso l'alto |
| spazio | tenuto, spara |

I dardi, verde oliva, scendono dritti (100 punti). I serpeggianti, arancioni, oscillano da un lato
all'altro e ogni tanto mirano un colpo (150). Gli assi, neri con le fasce rosse, si fermano a metà
e ne sparano tre alla volta (400). L'ondata 5 è un bombardiere grigio che regge quaranta colpi
(3000). Gli aerei restano nei 48 nodi centrali di uno schermo largo 80: un caccia li può
raggiungere tutti.

Ogni tanto, dall'ondata 2, passa un isolotto con sopra un **carro armato** (300, e sempre un
power-up): si gira verso il caccia più vicino e spara; è a terra, quindi passargli sopra non costa
niente.

Un aereo abbattuto -- loro, o il tuo e i suoi gregari all'ultima vita -- finisce in mare girando su se stesso e
fumando, e lascia un anello dove tocca l'acqua.

L'ondata 10, l'ultima, è la **fortezza volante** (20000): larga 40, sei torrette (500 l'una) che
si girano verso il caccia più vicino, e uno scudo -- l'anello di luci che le gira attorno. Finché
lo scudo è su, tutto quello che gli si spara dentro si spende su di lui; abbattuto (la barra
azzurra in cima allo schermo), resta giù dieci secondi, nei quali si possono colpire le torrette e
lo scafo (la barra rossa). A metà, lo scafo lancia anche anelli di colpi. Abbattuta lei, la
partita è vinta: i caccia escono dall'alto dello schermo più veloci di tutto quello che c'è sopra,
scendono sul piazzale dal mare e rullano fino alla loro piazzola, e i piloti sono di nuovo a piedi
nell'hangar.

Un colpo o un aereo che ti tocca costa una vita, e per due secondi e mezzo dopo niente ti tocca
(sulle punte delle ali lampeggia l'azzurro). Finite le vite si torna nell'hangar; quando ci sono
tutti la partita è finita e il record resta (`core.storage_set`).

## In più giocatori

Tutti volano le stesse ondate, ognuno con le sue vite, il suo punteggio e i suoi power-up. Per
partire assieme ognuno sale su un caccia: la partita parte quando il primo è in volo, e il suo
primo aereo arriva un secondo e mezzo dopo -- per allora gli altri sono su. Chi sale più tardi
entra nella partita com'è. Chi è abbattuto aspetta nell'hangar che la partita finisca. Con altri
in volo, una luce azzurra in coda segna il proprio caccia, e sotto il numero dell'ondata c'è
`PILOTS n`.

La partita si fa più dura per ogni pilota in volo oltre il primo:

| | ogni pilota in più | due | tre | quattro |
|---|---|---|---|---|
| quanto reggono un aereo, una torretta, lo scafo e lo scudo (un dardo continua a reggere un colpo) | +60% | x1,6 | x2,2 | x2,8 |
| aerei in un'ondata, nello stesso tempo | +50% | x1,5 | x2 | x2,5 |
| cadenza di fuoco di aerei e torrette | +25% | x1,25 | x1,5 | x1,75 |

Ogni volta meno di quanto vale un pilota in più: due caccia hanno il doppio dei cannoni ma lo
stesso cielo in cui schivare. Un'ondata si dimensiona quando parte e un aereo quando entra, quindi
chi sale o è abbattuto cambia le successive; la cadenza di fuoco segue chi vola adesso. Più aerei
vuol dire anche più power-up, che sono di chi li raggiunge.

## I power-up

Un aereo abbattuto ogni sei, ogni asso e il bombardiere (tre) ne lasciano uno, che scende lungo lo
schermo: passaci sopra. Quello che danno dura tutta la partita, tranne i tre che lampeggiano, che
finiscono.

| colore | | |
|---|---|---|
| verde | velocità | 5 m/s in più, tre volte |
| verde, lampeggia | turbo | 10 m/s in più per dieci secondi |
| rosso | potenza | due cannoni, poi tre, poi tre che sparano più in fretta |
| rosso, lampeggia | fuoco rapido | tutto spara al doppio della cadenza per dieci secondi |
| viola | missile a ricerca | un missile in più a salva (ogni 1,2 s), fino a quattro; ognuno toglie quanto tre colpi all'aereo più vicino |
| giallo | fuoco diagonale | due colpi in obliquo, poi quattro |
| azzurro, lampeggia | laser | dieci secondi (fino a trenta) di raggio al posto dei cannoni, attraverso ogni aereo sopra il caccia |
| bianco | gregario | un piccolo aereo a fianco che spara anche lui, fino a due |

Uno che il caccia ha già al massimo vale 500 punti.

## Com'è fatto

Il server tiene tutto quello che decide: dove sono aerei e colpi, chi è stato colpito. Alla
macchina del giocatore ogni colpo viene detto una volta, quando parte, e lei lo fa volare da sola
finché non le si dice che è sparito; missili, raggi e fortezza, che non vanno dritti, le vengono
detti dieci volte al secondo. Aerei, torrette e gregari sono entità del mod (`core.ecs.visible`);
i modelli sono l'aereo e il bombardiere del fronte con livree loro e una fortezza costruita per
questo gioco (`tools/make_shmup_models.py`, Blender), le immagini e i suoni sono di
`tools/make_shmup_assets.py`.

Il campo sta fermo sopra un mare di blocchi senza niente; quello che ci scorre sotto -- creste,
isole, nuvole -- lo disegna la macchina del giocatore (`core.render.pass`), senza fine, ogni isola
fatta a partire dal suo numero. L'unica isola di blocchi è la base, a sud del bordo basso
dell'immagine. Anche il decollo è della macchina del giocatore: il caccia è un modello disegnato
lungo un percorso e la telecamera è `core.fixed_view` chiesta di nuovo a ogni fotogramma, che
finisce esattamente dove poi la tiene `core.top_view_at`; solo allora il server fa del corpo un
caccia (`core.body_model`), gli dà il volo (`core.body_movement`, che un power-up di velocità
cambia) e lo mette sopra il campo. L'alto dell'immagine è +z, quindi la destra è -x.

Per provare, `TEST_WAVE` e `TEST_DROPS` in cima alla metà server di `mods/shmup` fanno partire la
partita dalla fortezza con trenta vite e fanno lasciare un power-up a ogni aereo.

## Quello che manca

Vedi `TODO.md`, "Platform game and arcade shooter (drafts)".
