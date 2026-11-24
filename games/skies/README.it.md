# Il mondo delle nuvole

*[Read in English](README.md) · [Leia em português](README.pt.md)*

Isole senza fine sopra un mare di nuvole: villaggi di uomini gufo, cristalli del cielo, correnti
ascensionali, e in fondo alla strada del cielo la Manta delle Nuvole, un boss per un giocatore o
per più d'uno. Chi c'è quando cade riceve il deltaplano della tempesta, da portare via. Una mod,
`mods/skies` (`side = both`): `layout.luau` il mondo, `init.luau` il terreno e il gioco sul
server, e sulla macchina del giocatore il mare di nuvole, le correnti, i colpi e la salute della
Manta. Le mod del sandbox sono collegate (robot, attrezzi, tiro con l'arco, armature,
deltaplano...), così borsa e armi vengono con te.

## Come arrivarci

Il suo cancello è sul bordo sud della piazza, a 69.5 21 26.5: un arco bianco come una nuvola con
delle assi davanti (`mods/gates`). Come il gioco a piattaforme sta nel file delle stanze: parte
quando qualcuno si avvicina al cancello e si ferma un minuto dopo che l'ultimo se n'è andato. Esci
da una porta sull'isola di casa, all'origine, guardando a nord lungo la strada del cielo.

## Il mondo

Una cella di 56 per 56 alla volta, ognuna funzione del seme e della cella (`layout.luau`):

* **Isole** nella maggior parte delle celle, larghe da 5 a 11, alte da 30 a 42; alcune collegate
  con un ponte alla vicina a est o a nord (assi larghe tre, pali di tronco). Alcune hanno un albero;
  molte hanno **cristalli del cielo** sull'erba, che si raccolgono a mano.
* **La strada del cielo**: un'isola in ogni cella dritto a nord di casa, un ponte da ognuna alla
  successiva, fino alle isole della Manta, 420 più a nord.
* **Villaggi**: un'isola larga su tre ha due capanne (assi, tronchi, tetto di lana) e tre uomini
  gufo.
* **Correnti ascensionali** in buona parte delle celle, sopra i vuoti, mai sopra un'isola: ognuna
  vaga lentamente, 9 blocchi intorno al suo posto. Si vedono come sbuffi azzurri che salgono a
  spirale, strisce di luce che corrono verso l'alto e un anello sulle nuvole. Chi ci entra (a 3,2
  dal centro) viene lanciato in su a 15,5 m/s fino a 58: da 24 a 58 in 3,5 s. Un modo per
  tornare su da una caduta, e col deltaplano un modo per salire.
* **Il mare di nuvole** da 6 a 22: le nuvole del cielo disegnate una seconda volta, più in basso e
  quasi compatte (`core.cloud_sea`, nuova nel motore). Un corpo che cade sotto 18 viene rimesso
  dove stava l'ultima volta (sull'isola più vicina se lì non c'è più niente) e perde 4.

Qui non si scava né si costruisce; un cristallo del cielo è l'unica cosa che si prende.

## Gli uomini gufo

Da Meshy (un'immagine di prova approvata, immagine in 3D, camminata e corsa dal rig automatico;
`tools/make_meshy_boss.py owlman` fa il resto: idle con la testa che gira, saluto, colpo d'ala,
planata, colpito, morte). Camminano per la loro isola e ogni tanto planano su un'altra entro 75
(l'animazione "glide", su un arco), e ci vivono per un po'. Un clic destro su uno: si gira e
saluta, dice quanto distano le isole della Manta e come aiutano le correnti; e scambia:

| dai | ricevi |
|---|---|
| 2 cristalli del cielo | un deltaplano |
| 6 lingotti di ferro | 3 cristalli del cielo |
| 5 cristalli del cielo | un diamante |
| 3 cristalli del cielo | 16 assi |

Un villaggio si popola quando qualcuno arriva entro 80 e si svuota quando nessuno è entro 130.

## La Manta delle Nuvole

12 m da una punta d'ala all'altra; 400 di salute, e il 60% in più per ogni giocatore in più vicino
alle sue isole. Mentre la si combatte, la sua salute è una barra in cima allo schermo di chiunque
sia entro 160.

1. **In alto**: gira a 58 intorno alle sue cinque isole, irraggiungibile. Ogni 6 s soffia una
   raffica contro qualcuno su un ponte (lo dice un attimo prima): una palla di vento che si vede
   volare da lei, e chi è ancora entro 3,5 da dove arriva viene spinto via a 12 m/s. Un clic
   destro su un parafulmine chiama un fulmine sul parafulmine e dal parafulmine dentro di lei (si
   vede, con un lampo nel cielo): il 12% della sua salute, e turbini. Scende di 3 a ogni
   parafulmine. Il primo parafulmine fa iniziare lo scontro.
2. **In basso**: colpiti quattro parafulmini, cade sull'isola centrale e ci gira intorno in
   picchiata da 3 a 8 sopra l'erba. Ora le armi contano; stare sotto di lei costa 3 al secondo e
   una spinta (un anello di vento). Ogni 12 s chiama turbini.
3. **Il crollo**: a un terzo della salute risale, i parafulmini si riaccendono e i ponti cadono in
   circa 20 s. Chi non ha un deltaplano ne riceve uno: due parafulmini colpiti, raggiunti in volo
   dal centro (le correnti tra le isole aiutano), la finiscono.

Mentre la si combatte **arrivano in volo tre uomini gufo** che girano intorno alle isole e colpiscono
il turbine più vicino (una striscia dorata) o, quando è in basso, beccano la Manta.

Quando cade, tutti quelli vicini ricevono il **deltaplano della tempesta**
(`glider:storm_glider`, `mods/glider`): un deltaplano come l'altro, e ogni 1,5 s in volo una
raffica lo solleva, così scende molto meno e va molto più lontano. Un minuto dopo ponti e
parafulmini tornano. `/manta` dice quante volte l'hai sconfitta. Nessuno entro 220 dalle sue isole
per un minuto: tutto torna all'inizio.

## Come è fatto

La Manta e il turbine vengono da Meshy (`tools/meshy_prop.py`, poi `tools/make_sky_creatures.py`:
tre ossa, le ali che battono, una picchiata, la morte). All'inizio volavano con la coda in avanti:
l'importatore lascia l'oggetto in quaternioni, dove una rotazione scritta in `rotation_euler` non
viene letta; ora lo script ruota direttamente i vertici. `tools/make_sky_sounds.py` fa la raffica,
i versi della Manta e il crepitio dei turbini; il tuono è quello di `mods/weather`.

Il mondo è alto 64 come ogni stanza: isole, nuvole e la Manta stanno tutte sotto.

## Limiti

* Non giocato a mano: le raffiche su un ponte, lo scontro in basso, il volo nella terza fase, la
  spinta del deltaplano della tempesta, il pannello e gli scambi di un uomo gufo con clic veri.
* Un'asse tolta da un ponte che crolla sparisce subito; non la si vede cadere.
* `core.player_launch` accetta al massimo 16 m/s per asse: una corrente solleva a 15,5.
