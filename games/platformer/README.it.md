# Il gioco a piattaforme

*[Read in English](README.md)*

Un gioco a piattaforme visto di lato, come quelli degli anni ottanta: due livelli, una stanza
bonus, i power-up, le loro regole e la telecamera. Due mod: `mods/platformer_core` (solo server: il robot, come
salta, i modelli dei tubi, dei power-up e delle creature) e `mods/platformer` (`side = both`: la stanza, i
livelli e le regole sul server; la telecamera e il conteggio sulla macchina del giocatore).

## Come ci si arriva

Nel gioco (il menù del titolo, o `tools/track.sh`) il suo cancello è sul lato sud della piazza, a
40.5 21 26.5: un tubo verde in un muro di mattoni sotto un blocco premio (`mods/gates`). Il mondo
non parte col server: sta nel file delle stanze, si avvia quando qualcuno si avvicina al cancello e
si ferma un minuto dopo che l'ultimo se n'è andato; il cancello è un portale fra stanze che prende
in prestito una bocca. `tools/platformer.sh` avvia il sandbox con questo mondo soltanto, dietro la
prima apertura (`tools/platformer.sh -- --room platformer` ci nasce dentro). In tutti e due i casi
si esce in una **stanza** con la parete di fondo di vetro: oltre il vetro, visto di lato, c'è il
primo livello. La porta a sinistra dà su un ponte con un buco, contornato di blocchi premio, e su
un tubo verde. **Il buco è il livello 1-1**: è un portale che scala (`core.portal_scale`), due per
due, su uno di uno per uno nel cielo sopra l'inizio del livello -- guardandoci dentro dal ponte si
vede il livello dall'alto. Chi ci si butta cade da quel cielo alto la metà, come il gioco lo vuole,
e la sua macchina sposta la telecamera da dietro di lui fino al lato mentre cade. Il tubo, sotto un
architrave blu, è l'1-2, e fa entrare chi ha superato l'1-1 (tenuto per nome nella memoria del
mondo).

## In un livello

La telecamera è di lato (`core.side_view`: `docs/MODDING.it.md`) e segue come lo schermo di un
gioco a piattaforme: ti sposti un poco di qua e di là prima che l'immagine scorra, poi scorre
morbida; un salto la muove meno di una caduta. I tasti sono quelli di un gioco a piattaforme:

| tasto | |
|---|---|
| A, D | cammina a sinistra e a destra |
| W o spazio | salta (2,4 in alto; 4,4 in lungo camminando, 7,2 correndo) |
| S | si accuccia; in cima al tubo alto dell'1-1, ci scende dentro |
| Shift | corre |
| F o tasto sinistro | lancia una palla di fuoco, con il fiore |
| E | rinuncia: torna alla stanza |

**Piccolo, grande, fuoco.** Si parte piccoli: metà dell'altezza del robot, e con numeri due volte
più grandi (`core.body_movement`, che si rimpiccioliscono con il corpo) così si corre e si salta
quanto da grandi. Un blocco premio con il power-up dà un fungo a chi è piccolo e un fiore a chi è
grande: il fungo sale dal blocco, scivola in avanti e cade dai bordi, e toccato fa diventare grandi
(il corpo tremola fra le due misure, poi cresce); il fiore resta sul blocco e dà le palle di fuoco,
due alla volta, che rimbalzano per terra e uccidono qualunque creatura. Colpiti da grandi o col
fuoco si torna piccoli e per due secondi niente colpisce; colpiti da piccoli, una vita.

Tre vite. Una caduta o un colpo da piccoli ne costano una e riportano, piccoli, all'ultima bandiera
passata (l'inizio, o quella blu a metà); finite le vite si torna nella stanza.

| creatura | | saltandoci sopra |
|---|---|---|
| germoglio (sprout) | un cactus su due piedi, che saltella avanti e indietro | muore, e rimbalzi |
| ronzone (buzzer) | un insetto viola che vola su e giù mentre avanza | muore, e rimbalzi |
| riccio (spikeshell) | una palla di spine rosse, che striscia | un colpo: va saltato, o bruciato |

Un blocco premio colpito da sotto con la testa dà una moneta, o il suo power-up, una volta sola;
sono profondi un solo cubo, nel piano del livello. Le monete si contano. **La stanza bonus**:
nell'1-1 il tubo alto tre con un gradino davanti porta giù -- sali in cima e premi S. Si affonda
nella bocca e si cade giù per il tubo, che continua nel terreno (con il davanti tagliato, così la
telecamera ti segue giù) e pende dal soffitto di una stanza di mattoni blu con tre file di monete.
Entrando nel tubo sdraiato in fondo alla stanza si risale per l'altra metà, di nuovo nel terreno,
e si esce dal tubo successivo del livello. Il palo alla fine di un livello ferma il tempo e dice tempo e monete: quello dell'1-1
porta all'1-2 con le vite e le monete che hai, quello dell'1-2 riporta nella stanza. Un livello
torna nuovo ogni volta che dentro non c'è nessuno.

L'1-2 è sotterraneo: mattoni blu sotto i piedi e sopra la testa con le lampade nel soffitto, un
muro scuro dietro, fosse con dei ripiani dentro, un tratto basso, tre ripiani a salire, e fuori
all'aperto per il palo.

## Com'è fatto

I livelli stanno sul piano z = 0,5 (profondi tre celle, così sembrano pieni) e vanno verso -x: la
telecamera è dal lato -z e guarda +z, il che mette l'avanti a destra nell'immagine, e la stanza è
dietro la telecamera, dove non entra mai nell'immagine. L'1-2 comincia 200 più avanti sullo stesso
piano. Tutto è una lista di scatole in `mods/platformer/init.luau` (`ground`, `pipe`, `prizes`,
`stairs`, `coins`, `foe`, `finish`), scritta da uno stadio di mapgen. I blocchi dei livelli fanno
un po' di luce propria: la telecamera guarda le facce che il sole non raggiunge mai.

- **I tubi** sono modelli fatti in Blender (`tools/make_platformer_models.py`): un tubo è tondo e
  largo due celle, quindi ogni pezzo è il modello di una cella (`core.node_model`) costruito
  attorno alla parete +x di quella cella, e le altre celle del tubo hanno un blocco che ferma il
  corpo e non si vede. Ogni pezzo ha un gemello che non ferma niente: un pozzo in cui passa un corpo
  è fatto di quelli finché il corpo è dentro. Un corpo che affonda in un tubo è tenuto da un lancio
  a ogni tick (`core.player_launch` non spinge mai un corpo in giù: gli porta la velocità almeno a
  0), poi cade; in salita è lanciato a ogni tick.
- **Il fungo, il fiore e la palla di fuoco** sono anche loro di Blender, nello stesso script.
- **Le creature** sono di Meshy: `tools/meshy_generate.py` (da testo a 3D con l'API di Meshy:
  un'anteprima, una rifinitura, il .glb in `assets/meshy/platformer`, con accanto la riga che l'ha
  chiesto) e poi `tools/meshy_prop.py`, che le dimensiona, le alleggerisce e ne cuoce la texture in
  `platformer_core`. Sono rigide -- senza animazioni -- e sono entità del mod
  (`core.ecs.visible`), mosse dal server; anche toccarle e saltarci sopra si decide lì, dalle
  posizioni che i giocatori dichiarano.
- **Immagini e suoni** li scrive `tools/make_platformer_assets.py`.

## Quello che manca

Vedi `TODO.md`, "Platform game and arcade shooter (drafts)".
