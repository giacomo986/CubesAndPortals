# L'Inferno

*[Read in English](README.md)*

Un mondo di caverne tra un pavimento e un soffitto di roccia madre, mari di lava in fondo,
glowrock appesa al soffitto e fortezze di mattoni le cui corsie fanno da ponte sulle caverne.
Niente sole: un cielo rosso, e l'unica luce è quella della lava e della glowrock. La stanza
`inferno` di `tools/track.sh`, raggiunta dalla cornice di ossidiana della piazza o da una
costruita da un giocatore.

Le sue mod: `inferno` (il terreno, la lava, le creature, tutto quello qui sotto),
`inferno_heart` (la prova della Chiave del Fuoco), `inferno_track` (la strada dei kart),
`inferno_ash` (`side = client`: cenere e braci nell'aria) e `infernoportal` (in comune con il
sandbox: le cornici). Ci sono collegate anche le mod di gioco del sandbox, così quello che un
giocatore si porta dietro vale qualcosa anche qui.

## Il terreno

| zona | dove | cosa |
|---|---|---|
| scoria | ovunque altrove | caverne di scoria, minerale di quarzo una cella su 90, sabbia di cenere sulle rive |
| campi di basalto | un rumore su x e z | basalto al posto della scoria, colonne dal pavimento al soffitto (fino a 30) |
| foreste di funghi | lo stesso rumore, l'altro estremo | gambi alti da 4 a 7 sotto cappelli luminosi larghi cinque |
| deserti di cenere | un secondo rumore | sabbia di cenere su ogni pavimento, glowrock la metà delle volte |

Lava sotto y 12; scorre (una sorgente e quattro livelli) e brucia 3 ogni mezzo secondo. Ogni
tanto una cascata di lava da una parete. Fortezze: ogni 96 nodi una corsia di mattoni larga
cinque lungo x o z, costruiti due tratti di 64 su tre, un baule ogni 64 contro la ringhiera.

## Cosa dà

| da | dà | per |
|---|---|---|
| bruto | pepita d'oro | nove un lingotto (`furnace:gold_ingot`, i suoi attrezzi) |
| spettro | lacrima di spettro | la resistenza al fuoco dell'alambicco (`mods/alchemy`): 3 minuti, lava e fuoco non bruciano |
| tizzone | bastone di tizzone | combustibile; macinato in 2 polveri di tizzone: con un lingotto di ferro un accendino, con un lingotto d'oro e vetro 2 lanterne di brace (luce 15) |
| minerale di quarzo | quarzo | quattro un blocco di quarzo; il centro di un comparatore (`mods/cinnabar`) |
| gambo di fungo | 4 assi di fungo | due assi, 4 bastoncini |
| un baule di fortezza | oro, bastoni, quarzo, lacrime, ossidiana, ferro, a volte lingotti d'oro, una resistenza al fuoco, un diamante | riempito la prima volta che si apre; uno messo da un giocatore resta suo |

Per distillare serve una fiaschetta d'acqua, e l'Inferno non ne ha: va portata da sopra.

## Il fuoco

Una palla di fuoco di una creatura che scoppia su un blocco (evento del motore `core:burst`)
accende un fuoco nella cella davanti alla faccia colpita: 3 s sulla roccia, 8 sul legno, sui
funghi e simili; non si propaga mai, e brucia 1 ogni mezzo secondo chi ci sta dentro. La
resistenza al fuoco lo tiene lontano.

## Due Inferni: quello aperto e i livelli dei siti

L'Inferno aperto è quello a cui portano una cornice di ossidiana e la piazza: un mondo solo per
tutti, le caverne, le fortezze e la via 1:8, e il cuore vicino all'arrivo come prova della piazza
(sempre d 1). Chi arriva da un sito sopravvivenza ha invece un livello suo, nello slot di quel
sito oltre x 8192 (mods/keys), così due siti non si incontrano mai e nessuno incontra il mondo
aperto. I pezzi vengono dal seme del sito, un'idea nuova alla volta, la più nuova per prima:

| d | i pezzi | bracieri |
|---|---|---|
| 1-2 | gallerie semplici | 2 |
| 3-4 | + un ponte su un lago di lava, bruti sopra, buchi di due da d 4 | 2 |
| 5-6 | + una galleria di spettri sulle cornici; colonne di basalto su e giù sopra la lava | 3 |
| 7-8 | + un braciere in fondo a una vasca di lava profonda sei: serve la resistenza al fuoco | 3 |
| 9-10 | uno qualsiasi, e niente pietra luminosa: fanno luce solo i bracieri | 4 |

Una stanza di partenza con l'uscita nascosta nel muro ovest (una lampada spenta sopra), i pezzi
verso est, ognuno chiuso da un braciere, poi il cancello e la sala. Accesi tutti i bracieri il
cancello si apre; chi entra nella sala ci torna dopo la morte, e il guardiano si leva, più
robusto del 15% per ogni passo di d, con d - 1 bruti e spettri. La sua morte chiude il livello per
chi è entrato nella sala: la chiave da d 4, l'uscita si apre, e una cella del pavimento della sala
si accende e li riporta alla partenza. Chi arriva in un livello dove non c'è nessuno lo trova da
capo: bracieri spenti, cancello chiuso.

In un livello una cornice di ossidiana si accende e non porta da nessuna parte (oltre x 8000 qui,
64000 sopra), e un baule di fortezza dà meno: probabilità a metà, al massimo metà quantità, mai
le cose rare.

## La via d'ingresso e quella del ritorno (1:8)

Un nodo qui vale otto nel mondo di sopra. Una cornice di ossidiana accesa con l'accendino nel
sandbox sposta lì la via della piazza, e l'Inferno costruisce la gemella a un ottavo della
distanza, all'altezza dove rompe meno. Una cornice accesa qui (vale anche l'ossidiana scavata
nell'Inferno) sposta lì la via del ritorno, e il sandbox costruisce la gemella otto volte più
lontano, sul terreno. Rotta una cornice accesa, il suo lato torna a casa (la piazza, o la camera
d'arrivo a 32 30 32) e l'altro lo segue. Ogni lato ignora l'apertura lontana che torna dove già
corrisponde.

## Suoni e aria

Attorno a ogni giocatore ogni 6-14 s un brontolio, la lava che ribolle sotto o un lamento
lontano; un fuoco che si accende (`tools/make_inferno_sounds.py`). Sulla macchina di ogni
giocatore 160 fiocchi di cenere scendono attorno all'occhio e uno su nove è una brace che sale.

## File

- `tools/make_inferno_textures.py`: le immagini dei blocchi (`games/inferno/textures`), tra
  cui la via del ritorno dei livelli
- `tools/make_inferno_sounds.py`: i quattro suoni
- `tools/make_held_models.py` (`ONLY=inferno:quartz,...`): gli oggetti in mano e le loro icone

## Provato (2026-10-31)

Headless e in Xephyr: le tre zone viste; un baule di fortezza a 104 30 53 rivolto alla corsia,
riempito con 9 pepite, 2 bastoni, 6 quarzi, ossidiana e ferro; un fuoco su un ponte; 4 s nella
lava a 20 su 20 con la resistenza al fuoco; una palla di fuoco contro un muro segnalata una volta
con la sua faccia (una prova unitaria del motore); una cornice accesa a 36 30 32 che dà la gemella
a 292 27 260, e tutte e due a casa dopo averla rotta. Non visti: una palla di fuoco di uno spettro
che accende un fuoco in gioco, una cornice accesa nel sandbox dopo la modifica, i suoni ascoltati.

I livelli (stesso giorno), headless: costruiti a d 1, 3, 5, 7 e 9, ogni braciere, cancello,
uscita nascosta e via del ritorno dove previsto, il cancello aperto coi bracieri accesi. In
Xephyr un livello d 7 percorso da script (la vasca, il ponte, la galleria), il guardiano levato
con la scorta, ucciso dallo script, il pavimento acceso, calpestato, e di nuovo alla partenza.
Non provati: un livello combattuto a mano, una cornice accesa in un livello, un baule aperto lì.
