# Il tower defense

*[Read in English](README.md) · [Leia em português](README.pt.md)*

Mostri degli altri mondi escono da una fenditura e camminano verso un cristallo; li fermi con
torrette fatte di quello che hai minato, barricate che allungano la loro strada e le tue armi. Da
soli o insieme. Due mod: `mods/defense_core` (il campo, la fenditura, il cristallo, i banchi, la
porta di casa) e `mods/defense_game` (le ondate, le torrette, le barricate, i record). Le mod del
sandbox sono collegate (robot, attrezzi, minerali, tiro con l'arco, armature, deltaplano...), così
borsa, armi e armatura vengono con te, e quello che danno le ondate torna a casa con te.

## Come arrivarci

Il suo cancello è sul bordo sud della piazza, a 25.5 21 26.5: un tratto di muro di pietra con la
luce di un cristallo sopra il varco (`mods/gates`). Come il gioco a piattaforme e lo sparatutto sta
nel file delle stanze: parte quando qualcuno si avvicina al cancello e si ferma un minuto dopo che
l'ultimo se n'è andato. Esci da una porta nel muro sud del campo, accanto al cristallo, guardando
il campo verso la fenditura.

## Il campo

49 di larghezza e 73 di lunghezza, chiuso da mura di mattoni di pietra alte cinque. All'estremità
nord la fenditura, una cornice di pietra nera con luce viola dentro, da cui escono i mostri;
all'estremità sud il cristallo sul suo piedistallo, due banchi ai lati della porta. Sempre giorno:
l'orologio è fermo a mezzogiorno.

## Come si gioca

* **Inizio**: clic destro sul cristallo. Ognuno riceve 24 barricate (e la prima volta in assoluto
  assi, ferro e ciottoli, quanto basta per una torretta a frecce), e la prima ondata arriva in
  30 s (un clic destro sul cristallo la manda subito).
* **Ondate**: 5 + 2 per ondata, la metà in più per ogni giocatore in più, ognuno il 12% più
  resistente di quelli dell'ondata prima, e il 35% per ogni giocatore in più. Zombi e demoni; dalla
  quarta ondata anche cyborg. Ogni quinta ondata un boss: il Guardiano dell'arena, poi il Macellaio
  della cripta, a turno. Un mostro al cristallo gli toglie 1 (un cyborg 2, un boss 10) dei suoi 20;
  a 0 la partita è finita e il campo si svuota. Un mostro con un giocatore entro 3,5 lo combatte per al
  massimo 10 s, e torna sulla sua strada quando è a 8; vicino al cristallo non combatte nessuno e
  va dritto al cristallo.
* **Tra un'ondata e l'altra**: 25 s, o un clic destro sul cristallo. Ognuno riceve ferro, assi,
  ciottoli e 8 barricate; dalla seconda ondata ghiaccio, dalla terza bacchette di brace, dalla
  quarta oro e (un'ondata sì e una no) rubini, ogni quinta diamanti. Restano tuoi.
* **Barricate**: si mettono sull'erba del campo tra il cristallo e la fenditura, alte due; i mostri
  ci girano intorno. Una che chiuderebbe la strada torna nella borsa. Rotta, ridà se stessa.
* **Torrette**: clic destro su un banco, scegline una; finisce nella borsa, e si mette sul campo
  come un blocco. Un clic destro su una mostra il suo livello e la potenzia (fino al 3). Rotta,
  torna al livello 1.
* **Mira**: le torrette a frecce, fulmine e portale scelgono un mostro. Nel loro pannello si
  sceglie quale: il più vicino (all'inizio), il più lontano, il più avanzato verso il cristallo, il
  più indietro, il più forte o il più debole (quello con più o meno salute rimasta, contata dal
  gioco a ogni colpo); tra pari, il più vicino. Braciere e gelo colpiscono tutti i mostri intorno.

| torretta | costa | potenziamento (al 2, al 3) | cosa fa (livelli 1/2/3) |
|---|---|---|---|
| frecce | 4 assi, 2 ferro, 4 ciottoli | 2 ferro, 4 ferro | il mostro più vicino entro 10/11/12: 5/7/10 ogni 0,8/0,65/0,5 s |
| braciere | 6 ciottoli, 2 bacchette di brace, 2 ferro | 2 bacchette, 3 | ogni mostro entro 3,5/4/4,5: 3/5/7 al secondo |
| gelo | 4 ghiaccio, 2 ferro | 3 ghiaccio, 5 | rallenta ogni mostro entro 6/7/8 al 55/45/35% del suo passo |
| fulmine | 2 oro, 1 rubino, 2 ferro | 1 rubino, 2 | 9/12/16 entro 9/10/11, e poi su altri 3/4/5 entro 4 l'uno dall'altro, tre quarti ogni volta, ogni 1,6/1,4/1,2 s |
| portale | 1 diamante, 1 oro, 4 ciottoli | 1 diamante, 2 | rimanda alla fenditura il mostro più vicino entro 5/6/7, ogni 10/8/6 s; mai un boss |

* **Record**: l'ondata migliore di ogni giocatore, e quella del campo con chi l'ha tenuta. `/defense`
  o un clic destro sul cristallo durante una partita li mostra.
* Nessuno qui per 20 s: la partita è finita e il campo si svuota.

## Cosa disegna la macchina del giocatore

`defense_game` gira su entrambi i lati (`side = both`). Il blocco sotto una torretta è
trasparente (`core.node_glass`): serve solo a fermare i corpi e a ricevere i clic. La macchina di
ogni giocatore ci disegna sopra il modello della torretta (`core.render.model`), un po' più grande
a ogni livello. Ogni torretta mira al mostro più vicino a sé. Le due che hanno un davanti stanno su
una base che resta ferma: la balestra e l'anello del portale ci ruotano sopra verso il bersaglio,
con dolcezza, la balestra con un rinculo quando tira e che si guarda intorno piano quando non
arriva nessuno da un po' (`tools/make_turret_parts.py` ha diviso in due i loro modelli). Braciere,
gelo e fulmine colpiscono tutto intorno e restano fermi. Ognuna ha una luce sua: la fiamma del braciere, il bagliore del cristallo di gelo, la sfera
rossa del fulmine che crepita, il vortice del portale. Quello che sparano si vede arrivare
(`core.render.fx`): la scia di una freccia e le scintille dove colpisce, un anello di fuoco e
fiamme su ogni mostro bruciato, il gelo che si allarga a terra, il fulmine che salta da un mostro
all'altro, un vortice dove un mostro viene preso e un altro alla fenditura da cui riesce. Il server
comunica ogni torretta (`T`), ognuna tolta (`R`), il campo svuotato (`C`) e ogni colpo (`F`, la
torretta e i punti colpiti).

## Come è fatto

Le torrette vengono da Meshy: un'immagine di prova ciascuna (testo in immagine), immagine in 3D
(meshy-6), poi `tools/meshy_prop.py --height 1.4`. Sopra ogni torretta una cella solida invisibile,
così nessun mostro ci sale sopra e scavalca una barricata. I mostri sono quelli dell'assalto
(zombi, demone, cyborg), il Guardiano dell'arena e il Macellaio della cripta, copiati con i loro
suoni; i suoni delle torrette sono quelli dello sparatutto.

Un mostro mandato in fondo al campo in un colpo solo (`core.mob_goto`) esaurisce la ricerca su un
campo pieno di barricate e si ferma a metà; così la strada è calcolata una volta
(`core.find_path`, ogni volta che una barricata o una torretta viene messa o tolta) come tappe ogni
sei celle, e ogni mostro va di tappa in tappa.

## Limiti

* Torrette e barricate vanno con la partita: una sconfitta svuota il campo.
* Il banco da lavoro del sandbox (`mods/craft`) sta sul campo a 39 20 47, incassato nell'erba.
