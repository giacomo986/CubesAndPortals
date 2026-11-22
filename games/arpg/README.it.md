# La landa (arpg)

*[Read in English](README.md) · [Leia em português](README.pt.md)*

Un action RPG isometrico punta e clicca: un paese, tre zone di aree e corridoi, la cripta sotto i
tumuli. Le mod:

- `arpg_core`: blocchi, il corpo del giocatore, le creature (tra cui il Macellaio), la gente del
  paese, ciò che un personaggio indossa e impugna (`gear_*.gltf`).
- `arpg_game`: la mappa, classi, abilità, oggetti, mercanti, missioni, materiali, l'interfaccia.
- `arpg_crypt`: i tre piani della cripta, il Custode delle tombe, l'uscita.
- `arpg_icons`: le icone dell'inventario (servite come file di un modello, vedi sotto).

`tools/arpg.sh --fresh` avvia un sandbox e questo mondo su un server; si entra dall'arco di pietra.

## Giocare

Tasto sinistro: cammina, colpisci, parla, raccogli. Tasto destro: l'abilità scelta con 1-4.
**Q** pozione di vita, **R** pozione di mana, **B** portale cittadino, **I** borsa,
**C** personaggio, **K** abilità.

- **La cintura**: tre caselle accanto ai globi, ognuna col suo tasto e quante ne restano, che si
  illumina quando la usi; quella della vita pulsa sotto un terzo di vita. Pozioni e oro si
  raccolgono passandoci sopra (o cliccando il nome della pozione); con la cintura piena (8 per
  tipo) un messaggio lo dice.
- **Oggetti**: ognuno ha un'icona dipinta, più ricca quanto più è raro: normale semplice, magico
  bagliore blu, raro bagliore giallo e scintilla, unico cornice d'oro con gemme rosse, set
  cornice verde.
- **Materiali**: minerale di ferro, pelle conciata, osso, polvere arcana, rubino, frammento
  d'anima, essenza di brace, scaglia di drago. Cadono dalle creature (ogni tipo i suoi), da barili,
  casse e urne, dai forzieri, dai campioni (rubini), da quelli con un nome e dai signori
  (frammenti d'anima; le scaglie di Alabrace), si raccolgono passandoci sopra e stanno nella
  sacca sotto la borsa (99 per tipo). Oggi li chiedono le missioni; creare, incantare e
  potenziare verranno.
- **I mercanti** hanno tre schede: COMPRA, RICOMPRA (le ultime dodici cose vendute, al prezzo
  ricevuto) e MISSIONE.
- **Missioni**: il guardiano del cancello, il fabbro, il gioielliere e l'alchimista hanno ognuno
  una catena, una alla volta, offerta quando il personaggio ha il livello: uccidere tot creature
  di un tipo (o campioni, o quelli con un nome), raccogliere tot di un materiale (consegnato), o
  il Macellaio. Un `!` sopra qualcuno vuol dire una missione da prendere, `?` una da consegnare;
  quelle prese sono elencate a sinistra. Una missione paga oro, esperienza e un oggetto almeno
  magico o raro.
- **Contorni**: la creatura sotto il puntatore ha un bordo rosso (la gente del paese oro pallido),
  quella che stai combattendo un bordo oro che pulsa: si vede cosa colpirà un clic e cosa stai
  colpendo (`core.outline`, `core.cursor_creature`, una funzione del motore che ogni mondo può
  usare).
- **Missioni di uccisione**: contano tutti i nomi di un tipo, perché più avanti uno scheletro si
  chiama arciere d'ossa; il pannello della missione li elenca (CONTANO). Un signore già ucciso
  quando prendi la sua missione torna nella sua tana.
- **Il Macellaio**: il signore della Desolazione grigia prima del suo signore, nella sua tana
  (area b1, livello 3), con quattro morti: enorme, veloce, una mannaia che picchia duro. Se entri
  nella sua tana mentre è vivo, un muro di luce rossa chiude la via del ritorno (niente portale
  cittadino) finché non cade.
- **L'aiutante del guerriero**: lo assumi dal guardiano del cancello (scheda AIUTANTE), ti segue e
  combatte secondo l'arma che gli dai: una lama da vicino, un arco da lontano, un bastone o una
  bacchetta col fuoco. Su quella scheda clicca gli oggetti della borsa per darglieli (i suoi otto
  riquadri, niente anelli, non sopra il tuo livello); clicca i suoi riquadri per riprenderli.
  Beve le tue pozioni di vita sotto due quinti della vita. Caduto, il guardiano lo resuscita, a
  caro prezzo, con ciò che indossava. L'equipaggiamento si vede addosso (`core.mob_wear`).
- **Trappole**: quasi ogni area dopo la prima ne nasconde una, che scatta passandoci vicino, una
  volta finché il mondo non si svuota: un'esplosione di fuoco dopo un anello rosso che si stringe
  (più di mezza vita), il pavimento elettrico a chiazze (spostati nel primo secondo di
  sfarfallio), un'onda di punte lungo una linea (attraversala, non seguirla), una nebbia gelida
  (rallenta al 40% per 4 s), una nube velenosa, o un'orda di quattro campioni intorno a te.
  Nessuna nella tana di un signore.
- **La cripta**: tre piani giù per scale (mezzi gradini), ognuno sigillato finché i suoi morti non
  sono a terra; chi salta in un piano più basso dal terreno sopra i muri viene rimesso in cima alle scale. Il Custode aspetta al terzo; quando cade si spezza il sigillo a est del piano, e
  un corridoio porta a una stanza col soffitto dove la telecamera scende all'altezza degli occhi,
  col portale di casa nella parete est.

## Modelli e icone, e come rifarli

Tutto da Meshy (crediti dell'account in `~/.claude.json`, vedi `tools/meshy_generate.py`), poi
Blender (`flatpak run org.blender.Blender --background --python ...`). Le sorgenti stanno in
`assets/meshy/`.

| Cosa | Strumento | Da |
|---|---|---|
| Icone dell'inventario (`arpg_icons/icon_*.png`, `icons.gltf`) | `python3 tools/make_arpg_icons.py` | tavole dipinte `assets/meshy/arpg_icons/*.png` |
| Armi, scudi e orb impugnati; la mannaia del Macellaio | `tools/make_arpg_gear_meshy.py` | `assets/meshy/arpg/sheet_*.glb` (image-to-3D delle tavole) |
| Pezzi indossati, robot e ogni specie | `python3 tools/make_arpg_armor_strips.py`, poi `tools/make_arpg_armor_meshy.py` | `assets/meshy/arpg/<riga>.glb`, adattati su `assets/meshy/arpg/fit_ref/` |
| I forzieri delle zone (chiusi, aperti) e il baule del deposito, come modelli dei loro blocchi | `tools/make_arpg_chests.py` | `assets/meshy/arpg/chests.glb` (image-to-3D di `chests.png`) |
| Il Macellaio, il Custode | `tools/make_meshy_boss.py -- butcher\|custode` | `assets/meshy/<nome>/walk.glb`, `run.glb` (rig di Meshy) |

- Le icone non sono item: gli item condividono l'atlante dei blocchi, la cui risoluzione la
  votano le immagini, e duecento icone a 96 px lo prendevano (il mondo diventava magenta). Le
  nomina invece un modello di un triangolo, e si disegnano come `model.arpg_icons_all.icon_<nome>.png`.
- Un pezzo impugnato va dove andava quello di `tools/make_arpg_gear.py` (stesso ingombro lungo
  l'asse della presa, stesso osso); i pezzi di quello script ora sono il riferimento, in `fit_ref`.
- Un pezzo indossato è stirato sull'ingombro del vecchio pezzo per quel corpo (guanto e stivale
  sulla metà del loro lato), messo appena fuori dalla sua superficie (un pezzo dipinto è un
  rilievo, e quasi tutto finiva dentro il corpo), e ogni vertice prende le ossa del vecchio
  vertice più vicino.
- I boss: girati verso +Y e dimensionati (Macellaio 2,9 m, Custode 2,6 m), ciò che impugnano
  messo come deve apparire a braccia abbassate, saldati, ridotti a 6000 triangoli, il colore
  cotto su 512 px; camminata e corsa di Meshy tenute, idle, attacco (e il lancio del Custode),
  colpito e morte animati qui come rotazioni sugli assi del mondo. Ogni clip:
  `~/.cache/cnp_look/<nome>_clips.png`.
- Ogni file resta sotto il MB che un file media può pesare; gli script lo controllano.

## Limiti noti

In [TODO.md](../../TODO.md) (Mods, "Arpg"): i materiali servono solo alle missioni;
l'equipaggiamento con texture non prende più il colore delle rifiniture di unici e set; i pezzi
indossati sono un po' accartocciati dove avvolti; i boss si colpiscono nel riquadro della loro
T-pose; i numeri del Macellaio sono stime.
