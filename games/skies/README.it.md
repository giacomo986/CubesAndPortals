# Le isole sospese

*[Read in English](README.md) · [Leia em português](README.pt.md)*

Sei isole sopra un mare di nuvole, ponti tra di loro, e la Manta delle Nuvole: un boss per un
giocatore o per più d'uno, e il deltaplano della tempesta per chi c'è quando cade, da portare via.
Una mod, `mods/skies` (`layout.luau` le isole e i ponti, `init.luau` il terreno e lo scontro). Le
mod del sandbox sono collegate (robot, attrezzi, tiro con l'arco, armature, deltaplano...), così
borsa e armi vengono con te.

## Come arrivarci

Il suo cancello è sul bordo sud della piazza, a 69.5 21 26.5: un arco bianco come una nuvola con
delle assi davanti (`mods/gates`). Come il gioco a piattaforme sta nel file delle stanze: parte
quando qualcuno si avvicina al cancello e si ferma un minuto dopo che l'ultimo se n'è andato. Esci
da una porta sull'isola di casa, guardando a nord verso le altre.

## Le isole

| isola | dove (x y z) | cosa |
|---|---|---|
| casa | 0 40 -92 | la porta, una rastrelliera di deltaplani |
| sud | 0 40 -46 | un parafulmine |
| centro | 0 40 0 | la più larga (13), una rastrelliera; dove si combatte la Manta |
| nord, est, ovest | 0 42 46, 46 38 0, -46 44 0 | un parafulmine ciascuna |

Ponti di assi, larghi tre, con pali di tronco ogni quattro: da casa a sud, e dal centro a ognuna
delle quattro; quelli a nord e a ovest hanno un buco di due da saltare. Sotto, a 14, le nuvole; un
corpo che cade sotto 20 viene rimesso dove stava l'ultima volta (sull'isola più vicina se quel ponte
non c'è più) e perde 4. Le rastrelliere danno un deltaplano (`mods/glider`) a chi non ne ha.

## La Manta delle Nuvole

12 m da una punta d'ala all'altra; 400 di salute, e il 60% in più per ogni giocatore in più.

1. **In alto**: gira a 58 intorno alle isole, irraggiungibile. Ogni 6 s soffia una raffica contro
   qualcuno su un ponte (lo dice un attimo prima): spinto a 12 m/s lontano da lei. Un clic destro
   su un parafulmine le scaglia contro un fulmine (il 12% della sua salute) e due turbini (uno in più
   per ogni giocatore); scende di 3 a ogni parafulmine. Il primo parafulmine fa iniziare lo scontro.
2. **In basso**: colpiti quattro parafulmini, cade sull'isola centrale e ci gira intorno in
   picchiata da 3 a 8 sopra l'erba. Ora si colpisce con le armi; stare sotto di lei costa 3 al
   secondo e una spinta. Ogni 12 s chiama turbini (uno, e uno in più ogni due giocatori).
3. **Il crollo**: a un terzo della salute risale, i parafulmini si riaccendono e i ponti cadono in
   circa 20 s. Chi non ha un deltaplano ne riceve uno: due parafulmini colpiti, raggiunti in volo
   dal centro, la finiscono (il 17% ciascuno).

Quando cade, tutti quelli nel mondo ricevono il **deltaplano della tempesta**
(`glider:storm_glider`): un deltaplano come l'altro, e ogni 1,5 s in volo una raffica lo solleva
(`core.player_launch`, 6 m/s in su), così scende molto meno e va molto più lontano. Funziona in ogni
mondo che ha `mods/glider`. Un minuto dopo ponti e parafulmini tornano e la Manta riprende a girare.
`/manta` dice quante volte l'hai sconfitta. Nessuno qui per 30 s: tutto torna all'inizio.

## Come è fatto

La Manta e il turbine vengono da Meshy (un'immagine di prova, immagine in 3D con meshy-6,
`tools/meshy_prop.py`); `tools/make_sky_creatures.py` dà a ciascuno tre ossa (corpo e due ali, ogni
vertice legato a un'ala secondo quanto è lontano dal centro) e le animazioni: idle, walk e run (le
ali che battono), attack e shoot (una picchiata), death (le ali si chiudono e si rovescia).
`tools/make_sky_sounds.py` fa la raffica, i versi della Manta e il crepitio dei turbini; il tuono è
quello di `mods/weather`.

La Manta è un lavoratore che vola: la mod la mette dove deve stare a ogni tick (`core.mob_place`),
e i colpi che riceve fuori dalla fase bassa le vengono restituiti.

Il mondo è alto 64 come ogni stanza: isole, nuvole e la Manta stanno tutte sotto.

## Limiti

* Non giocato a mano: le raffiche, i buchi nei ponti, il volo nella terza fase e la spinta del
  deltaplano della tempesta non sono stati provati con tasti veri (la spinta è del server su un
  corpo che muove il client).
* Un'asse tolta da un ponte che crolla sparisce subito; non la si vede cadere.
