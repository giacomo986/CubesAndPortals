# Il fronte

Un campo di battaglia nello stile dei grandi sparatutto a squadre: due basi,
tre punti da prendere, rossi contro blu. Due mod: `mods/front_core` (solo
server: il campo, i blocchi, come ci si muove) e `mods/front_game`
(`side = both`: squadre, armi, granate, bot e punti sul server, HUD ed
effetti sul client).

## Come ci si arriva

`tools/track.sh` (o il menù del titolo) avvia il sandbox con dieci stanze; la
porta del fronte, una porta di bunker fra sacchi di sabbia sotto una bandiera
rossa e una blu, sta a 90.5 21 58.5 (`mods/gates`, sedicesima bocca del
sandbox). Si entra nella **tribuna**: una stanza sul crinale ovest, a metà
strada fra le due basi e 16 nodi sopra il campo, con una vetrata da cui si
guarda la battaglia. Chi è in tribuna guarda e basta: non è di nessuna squadra,
i bot non lo vedono. Sulla parete di fondo ci sono due portali, uno rosso e uno
blu, e dentro ciascuno si vede la sua base. Chi entra in quello rosso esce nel
bunker della base rossa e combatte per i rossi; lo stesso con il blu. Dal
portale del bunker si torna in tribuna e si smette di combattere; da lì si può
scegliere di nuovo. La porta da cui si è entrati, in fondo a sud della
tribuna, riporta sulla piazza. Il fronte non dà chiavi.

Nel gioco finito la porta si apre solo a chi ha battuto l'Ospite nel
laboratorio (`mods/keys`: il laboratorio manda `keys:ending`, tenuto per ogni
stanza in `ending:<chi>`). Per ora `FRONT_OPEN = true` in `mods/keys/init.luau`
la lascia aperta a tutti, per provarla; `false` è il gioco come uscirà.

## Il campo

256 × 256 nodi chiusi da un crinale. Base rossa a sud (128, 24), blu a nord
(128, 232): un cortile di cemento con le strisce della squadra, sacchi di
sabbia, un bunker e il pennone. In mezzo, sulla strada fra le basi:

| punto | dove | cos'è |
|---|---|---|
| A | 48, 128 | una fattoria fra muretti di pietra, un fienile di legno |
| B | 128, 128 | il villaggio all'incrocio: sei case di mattoni, una piazza col pozzo |
| C | 208, 128 | un bunker di cemento su una collina, sacchi di sabbia tutt'attorno |

Due muri di pietra alti 2 attraversano il campo a z 86 e 170, con varchi
sulla strada e ogni tanto. Tutto si può far saltare, tranne il fondo e il
crinale.

## Come si gioca

* **Ticket**: 150 a squadra. Ogni morte ne costa uno; chi tiene più punti fa
  perdere all'altra un ticket ogni 4 s (2 s con tutti e tre). A zero la
  squadra perde il round, e dopo 10 s ne comincia un altro.
* **Punti**: chi sta entro 10 nodi conta; ogni soldato in più dell'altra
  squadra porta avanti la presa di 10 al secondo, a 100 il punto è preso e la
  bandiera cambia colore.
* **Armi** (in mano dall'inizio): fucile e mitragliatrice sparano finché si
  tiene il sinistro, la pistola e il fucile da cecchino un colpo a pressione,
  R ricarica. Una testa vale il doppio. Dal fianco i colpi si allargano (il
  mirino lo mostra); col **destro** si mira: l'arma sale al centro della vista,
  la vista si stringe, i colpi vanno dove si guarda e si cammina a 1.8 m/s. Il
  cecchino, mirando, guarda nel cannocchiale (5x) e non sbaglia. Niente fuoco amico. Nel cortile della propria base munizioni
  e granate si ricaricano.
* **Granate**: rimbalzano, scoppiano dopo 3 s, scavano una sfera di 2.6 e fanno
  cadere quello che resta appeso sopra il buco.
* **Bot**: ogni squadra è riempita fino a 8 con soldati comandati dal mod
  (si vede l'arma che tengono in mano);
  vanno di punto in punto e ogni tanto lanciano una granata. Vedono solo
  davanti (un cono di 110 gradi, entro 48 nodi, 90 col cecchino) o a meno di
  4 nodi; chi viene colpito si gira verso il colpo. Prima del primo sparo
  passa un attimo (0.45-1.1 s), sparano a raffiche, all'inizio larghi e poi
  più stretti, e un colpo loro fa tre quarti di quello di un giocatore.
  Quattro modi di combattere, a turno: la **sentinella** si ferma ogni tanto
  a guardarsi attorno, preferisce il cecchino e prende i nidi, i mortai e gli
  obici che trova; l'**assaltatore** corre addosso a chi vede con la
  mitragliatrice; il **prudente** va prima al riparo, si sporge per una
  raffica col fucile e torna dietro; l'**anticarro** comincia col
  lanciarazzi. Mortai e obici dei bot tirano su quello che un bot della
  squadra ha visto negli ultimi 3 s. Ognuno tiene la sua arma
  finché ha colpi, poi passa alla successiva (alla fine una pistola
  raccattata).
* **Bot sui mezzi**: guidano jeep e carro verso il loro punto, aspettano un
  attimo che salgano i compagni vicini; la jeep li lascia al punto, il carro
  resta e si ferma a sparare col cannone a chi vede entro 70 (un po' storto,
  ogni 5.5-7 s). Pilotano l'elicottero (a 18 sopra il punto, razzi su
  quello che hanno davanti), l'aereo e il bombardiere (in giro sopra il
  punto, bombe dove cadrebbero su quello che la squadra vede). Salgono alle
  armi e sui posti liberi di un mezzo della loro squadra fermo vicino, anche
  guidato da un giocatore, e scendono se nessuno guida; alla contraerea
  quando un nemico vola vicino. Lasciano jeep e carro a un giocatore della
  squadra entro 25 nodi.

## Modelli

`tools/make_front_models.py` (Blender, con gli aiuti di
`tools/make_fps_models.py`): le braccia del robot in prima persona con
pistola, fucile, mitragliatrice e granata; le armi in mano; i soldati rosso e
blu con idle, walk, run, fire, throw e death.

## Mezzi e armi pesanti

Si sale e si scende con **F** vicino a un mezzo, sul posto libero più vicino (da
dietro la jeep si sale dietro); **1-9** cambiano posto, anche guidando, senza
cambiare l'arma in mano. `/mezzo <tipo>` mette un mezzo davanti a te, per le
prove. Il
primo posto guida e ha l'arma del mezzo; WASD guida a terra, spazio e shift
salgono e scendono, il mouse governa quello che vola.

| | dove | posti | armi |
|---|---|---|---|
| jeep | due per base | 4 | mitragliatrice dietro |
| carro | uno per base | 2 | cannone (chi guida), mitragliatrice sulla torretta; sfonda i muri spingendoci contro |
| elicottero | a ovest della base | 3 | razzi (chi pilota), mitragliatrice al portellone |
| aereo | sulla strada a nord della base | 1 | mitragliatrici, bombe col destro |
| bombardiere | in fondo alla strada nord, dietro l'aereo | 2 | chi pilota sgancia una fila di sei bombe (un palo rosso segna dove cadrebbero), torretta di mitragliatrice sul dorso |
| contraerea | due nel cortile della base | 1 | missili solo contro quello che vola: si tiene il mirino su un elicottero o un aereo nemico 1.2 s (LOCKING), poi LOCKED e quattro missili che lo inseguono |
| nido di mitragliatrice | ai sacchi di sabbia lungo le strade | 1 | |
| mortaio, obice | nelle basi | 1 | tirano dove guarda il mirino (un palo rosso lo segna) |

La fanteria ha anche il lanciarazzi. Un mezzo distrutto esplode: il
relitto brucia, perde i pezzi (la torretta del carro vola via, la jeep si
ribalta) e in 5 s sprofonda; ne torna uno nuovo a casa dopo 30 s.

## Ancora da fare

Due mezzi non si urtano tra loro e un giocatore a piedi passa attraverso un
mezzo: TODO.md, "The front's machines". Un bot su un mezzo è rimesso al suo
posto dal mod a ogni tick (i posti del motore sono dei giocatori).

## Suoni

`tools/make_front_sounds.py` li sintetizza (spari, esplosioni, motori,
rotore, le voci dei soldati) in ogg accanto al mod.
