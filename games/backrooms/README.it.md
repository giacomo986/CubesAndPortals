# Le backrooms

*[Read in English](README.md) · [Leia em português](README.pt.md)*

Un labirinto senza fine di stanze che nessuno ha costruito, su tre livelli
impilati e un quartiere infinito sopra, con porte rare che portano da un livello all'altro. Il mondo è
`mods/backrooms/init.luau` (server), più `backrooms_exit` (l'uscita) e
`backrooms_hand` (la torcia, server e client).

## Come ci si arriva

`tools/track.sh` (o il menù del titolo) avvia il sandbox con sette stanze;
il sesto cancello della piazza, un muro di carta da parati gialla con un neon
sopra, sta a 16.5 21 28.5 (`mods/gates`). Si esce nella stanza all'origine
del Level 0, quattro passi davanti al portale di ritorno. Un secondo portale
di ritorno è nella stessa stanza delle poolrooms, rivolto a +x: lo trova solo
chi è salito fin lì.

## I livelli

Il mondo è alto 96 celle (`--world-top 96`, che `tools/track.sh` e il menu del titolo passano);
i livelli stanno uno sopra l'altro. F è la prima
cella d'aria sopra il pavimento.

| livello | F | aria | materiali | luci |
|---|---|---|---|---|
| Level 1 | 5 | 6 | cemento, lastre, pozzanghere | 2 lampade calde a stanza, 25% delle stanze spente |
| Level 0 | 21 | 5 | carta da parati gialla, moquette, controsoffitto | 4 neon a stanza, 16% delle stanze spente |
| Poolrooms | 37 | 9 | piastrelle bianche, piscine azzurre | 5 lampade a stanza, mai spente |
| Il quartiere infinito | 66 | aperto | asfalto, marciapiedi, prati, la stessa casa ripetuta | lampioni arancioni, uno su cinque spento; una luce accesa in una casa su quindici |

Gli atri salgono nel vuoto sopra il loro livello: soffitto a 19 nel Level 1,
a 33 nel Level 0, a 62 nelle poolrooms.

Il nome del livello compare sullo schermo quando ci si entra
(`level_at`: dai piedi, y ≥ 63 il quartiere, ≥ 33 poolrooms, ≥ 17 Level 0, sotto Level 1).
Niente si scava e niente si costruisce.

Un mondo fatto alto 64 (prima della 0.21.0-alpha.4) tiene i suoi tre livelli e la sua pianta: il
mod chiede `core.world_height()` e aggiunge il quartiere, le sue scale, i buchi e i portali solo da
96 in su. Per averlo i mondi delle backrooms vanno rifatti (`tools/track.sh --fresh`).

## Il quartiere infinito

Sopra le poolrooms, il terreno a 65 e la terra sotto. Un isolato di 32 per 32 celle si ripete
senza fine: strade larghe 4 intorno con un marciapiede per lato, lampioni lungo i marciapiedi ogni
10 celle, e dentro quattro lotti tutti uguali: un prato, un vialetto fino alla porta, una cassetta
della posta, uno steccato bianco dietro e ai lati, un piano di perlinato chiaro con le finestre
buie, una porta senza battente e un tetto a gradoni. I due lotti a ovest guardano a ovest, i due a
est a est. Le case sono vuote e buie; sopra non c'è cielo, solo il buio. Non ci vive niente.

Ci si sale da una tromba di scale dalle poolrooms che esce dal terreno (in un prato, in strada o in
una casa), se ne cade in una piscina delle poolrooms da un buco, o ci si passa da un portale: il
quartiere è accoppiato con le poolrooms e con il Level 0.

## I livelli nuovi, a distretti

Il mondo è diviso in distretti di 128 x 128 celle. In tre distretti su dieci un livello nuovo sta
al posto di uno dei tre, alla stessa altezza, così scale, buchi e portali lo incontrano allo
stesso modo (`theme` in `mods/backrooms/init.luau`). I distretti intorno all'entrata e all'uscita,
e i siti survival, hanno sempre i livelli vecchi.

| livello | al posto di | com'è |
|---|---|---|
| L'hotel | Level 0 | moquette rossa, damasco crema, una lampada calda per stanza, corridoi lunghi pieni di porte che non si aprono |
| La festa | Level 1 | muri rosa, pavimento di coriandoli, luci rosa, mai buio, palloncini sotto il soffitto (`balloon_*.gltf`, col filo che pende), torte sui tavoli, e i pagliacci |

Il nome compare sullo schermo quando ci entri. Nell'hotel non c'è nessuno; nella festa ci sono i
pagliacci (sotto).

## Le backrooms profonde

Un mondo a sé (stanza `backrooms_deep`, gli stessi mod senza `keys` e `backrooms_exit`): la porta
è nella stanza (-6, 5) del Level 0, a 50 celle dall'entrata, un vano vestito del rosa della festa
(la terza apertura di questo mondo, `--portal-to @backrooms_deep/0`). Tutti e due i vani della
stanza all'origine delle backrooms profonde riportano qui.

Ha anche il quartiere infinito, quando è fatto alto 96. Lì due distretti su tre sono un livello
nuovo, i portali sono di più (una colonna di chunk su due)
e non portano su o giù: ogni dieci minuti i siti di ogni blocco di 8 x 8 regioni (256 celle di
lato) vengono mescolati e accoppiati, e i due vani di un sito portano ai due dell'altro, incrociati.
Attraversando un vano si finisce fino a 256 celle più in là, in un altro livello, e lo stesso vano
più tardi porta altrove.

## Come si genera

Uno stage di mapgen (`backrooms:rooms`) per colonna di chunk (32×32). La
pianta è funzione di x e z soltanto, non del seme: le backrooms sono le stesse
per tutti, e il tick trova i portali senza aver visto girare il mapgen (un
mondo caricato dal file non ne fa girare).

- **Stanze.** Una griglia di stanze di 8 celle. Il muro fra due stanze c'è o
  no secondo un hash (Level 0 76%, Level 1 42%, poolrooms 36%); se c'è ha
  sempre una porta larga 2, così da ogni stanza si arriva a tutte le altre.
- **Corridoi.** File di 4 stanze lungo x (o lungo z) diventano un corridoio
  largo 3 e lungo 32, i lati pieni, una porta per lato dove il muro della
  griglia ha la sua, una luce al centro (Level 0 22%, Level 1 30%, poolrooms
  11%, lì un canale allagato).
- **Atri.** Ogni tanto un blocco di 2×2 stanze è un unico ambiente alto fino
  al soffitto degli atri (Level 0 9%, Level 1 9%, poolrooms 12% dei blocchi):
  gallerie larghe 3 lungo i muri, un piano sopra l'altro (uno nel Level 0 e nel
  Level 1, due nelle poolrooms), un parapetto sul bordo interno, una rampa di
  scale lungo +x per salire a ogni galleria (la prima da terra lungo la galleria
  sud, la seconda dalla galleria ovest lungo la nord), lampade nel soffitto e
  nei pavimenti delle gallerie, che fanno luce sopra e sotto. Nelle poolrooms
  c'è una piscina in mezzo. Mai su stanze con scale, buchi o portali.
- **Cose in una stanza.** Level 0: pilastri; Level 1: pozzanghere e pilastri;
  poolrooms: piscine e scivoli.
- **Piscine.** Quadrate (2 di profondità), grandi quanto la stanza o a corsia
  lunga la stanza (3), canali nei corridoi (2), e quelle sotto i buchi. Tutte
  hanno un guscio, una scaletta d'acciaio per uscire sul lato -x (nell'acqua
  `backrooms:ladder_wet`, un liquido col modello della scaletta, così la superficie non ha un buco
  intorno; le scalette delle piscine fatte prima diventano così mentre si caricano) e, nelle
  poolrooms, un bordo di piastrelle larghe e luci nelle pareti sott'acqua.
- **Scivoli.** Un canale blu che scende di una cella per cella, fatto di pezzi
  dritti (`slide.gltf`) e di curve di un quarto di giro (`slide_turn_a.gltf`
  entra dal lato +x, `slide_turn_b.gltf` dal lato -x; escono davanti). Il mod
  li posa come una lista di direzioni (`lay`): dove la direzione cambia mette
  una curva, e restituisce la cella dove il corpo viene lanciato in acqua.
  - Nelle stanze: una torre di piastrelle con la scaletta e quattro celle di
    scivolo, dritto fino a una piscina lungo il lato basso della stanza, o con
    una curva fino a una piscina lungo il lato ovest (metà e metà).
  - Negli atri delle poolrooms: dalla prima galleria, da una piattaforma
    accanto alla galleria est, dieci celle giù lungo il lato est, curva, lungo
    il lato sud, curva e dentro la piscina centrale.
  
  Uno scivolo è un nastro veloce in discesa (`core.node_conveyor` 5, 1.25,
  -1), non solido: il motore ci fa scendere il corpo dentro, seduto sulla
  linea del letto (`chute_at`, `src/core/physics/aabb.cpp`), dritto o lungo
  il quarto di cerchio della curva (riconosciuta dallo scivolo una cella
  sopra che entra dal lato), da 2 m/s fino a 5, riportandolo al centro del
  canale. Lo fa la fisica del client come un passo qualunque, quindi senza
  scatti; alla fine del canale il corpo vola avanti con la velocità che ha
  e cade in piscina, e il mod suona il tonfo. Il salto fa uscire. La caduta
  dentro uno scivolo non conta. In curva il corpo e la vista girano con il
  canale; gli altri vedono chi scende seduto rivolto lungo lo scivolo,
  qualunque cosa guardi (il server lo manda con la direzione del canale). Il giocatore che ci scende fa la clip
  `slide` del robot (seduto, gambe avanti, braccia in alto) e ha l'occhio
  più basso, scendendo piano.
- **Scale.** Una tromba delle scale sale di 16 celle dal Level 1 al Level 0
  e dal Level 0 alle poolrooms: due rampe affiancate lungo z (x 1..2 sale verso
  +z, x 5..6 verso -z) con un pianerottolo a ogni capo, gradini da mezza cella
  che si salgono camminando, il soffitto aperto e il pozzo murato fino al
  pavimento di sopra, lampade nei muri del pozzo ogni 4 celle. L'ultima rampa sbuca da
  un'apertura 2×4 nel pavimento della stanza di sopra. Sotto le rampe si
  cammina.
- **Su al quartiere.** La stessa tromba di scale dalle poolrooms sale 29 celle; l'uscita è tagliata
  nel terreno del quartiere e nella terra sotto, e quello che ci sta sopra (il muro di una casa) è
  sgombrato per l'altezza di un corpo. Circa una stanza su sessanta; una su settanta ha un buco
  nell'altro verso.
- **Buchi.** Un buco 2×2 nel pavimento delle poolrooms (verso il Level 0) o
  del Level 0 (verso il Level 1), un pozzo murato, e sotto una piscina
  profonda 2: l'acqua azzera la caduta (`session.cpp`, `fall_reset` in un
  liquido), si arriva con la vita piena.
- **Portali.** In una colonna di chunk su tre (`PORTAL_CHANCE`) una stanza ha
  una porta illuminata con un portale vero, e la stessa stanza di un altro
  livello ha l'altro capo: si esce sopra o sotto il punto in cui si è entrati.
  La cornice è vestita come il livello dove porta. Si aprono
  (`core.portal_open`, `core.portal_link`) solo le coppie entro 40 celle da un
  giocatore, al massimo 7: le aperture sono 16 e due sono del mondo.

Circa una stanza su dodici ha scale o un buco. La stanza all'origine e quelle
dei portali restano sempre stanze normali. Le scale e i pianerottoli sono del
livello da cui partono: moquette con il bordo d'ottone nel Level 0, piastrelle
con il bordo azzurro nelle poolrooms, cemento con la riga gialla nel Level 1.

**Una regola dello stage:** una colonna scrive solo dentro sé stessa, e i muri
sui lati alti di un atrio o di un pozzo delle scale (x0 + 16, x0 + 8...) possono
stare nella colonna accanto. Li alza la stanza vicina, di cui sono i lati bassi:
la prima versione degli atri aveva il muro alto mancante, e dall'atrio si
vedeva il vuoto fra i livelli.

## Luce

La luce dei blocchi va da 15 alla lampada e scende di 1 a blocco; il motore
la disegna lineare, con un minimo del 15%. Il mod:

- mette un neon o una lampada **spenti** dove una stanza è al buio (si vedono,
  non fanno luce);
- fa **sfarfallare** i neon del Level 0 e le lampade del Level 1 attorno a chi
  c'è: ogni 0.3 s, per ogni giocatore, al 35% una lampada accesa di una stanza
  vicina si spegne per 0.08-0.38 s, una o due volte. Ogni cambio rifà la luce
  della cella (`core.set_block` mentre il mondo gira): pochi al secondo;
- illumina i corridoi al centro, i pozzi delle scale e dei buchi, gli atri
  dal soffitto e dai pavimenti delle gallerie, le piscine da sott'acqua.

Le creature escono solo dove la luce è 7 o meno, quindi nelle stanze spente. **Uno Smiler spegne
le luci** della stanza in cui si trova: le lampade del suo livello nel soffitto di quella stanza si
spengono finché c'è, e tornano un secondo dopo che se n'è andato.

## Chi ci vive

Due creature, `core.register_mob`, escono dove la luce è 7 o meno:

| creatura | dove | vita | velocità | colpo | come muore |
|---|---|---|---|---|---|
| Bacteria | Level 0, sulla moquette | 30 | 3.4 | 4 | barcolla e cade rigido in avanti |
| Smiler | Level 1, sulle lastre | 20 | 2.6 | 3 | chiude il ghigno e scende a terra girandosi |

Clip: idle, walk, run, attack, death. Nelle poolrooms non c'è nessuno. Morendo un
Bacteria lascia a volte un filamento, uno Smiler un dente: i pezzi della
bussola dell'uscita (vedi sotto).

**I pagliacci** vivono nella festa e vengono solo mentre nessuno li guarda. Sono worker
(`fight "worker"`), fatti uscire dal mod e mai dal buio: ogni 4 s, a chi è in un distretto della
festa, al 40% ne esce uno 3-5 stanze alle sue spalle, al massimo tre intorno a un giocatore. A ogni
tick un pagliaccio che un giocatore vede -- entro 45° dallo sguardo, niente in mezzo
(`core.raycast`) -- si blocca nella posa `frozen`, braccia tese; non visto da nessuno, va verso il
giocatore più vicino a 5 m/s e colpisce per 5. Non lascia mai il livello della festa: fuori
sparisce. Vita 40; clip idle, walk, run, attack, frozen, death.

## Suoni

ElevenLabs, text to sound (`eleven_text_to_sound_v2`, 50 crediti l'uno,
flow `2zuOw1wXkyasR0eBFnRn`), convertiti in ogg mono con ffmpeg. I versi
delle creature li suona il motore per nome (`<creatura>_say`, `_hurt`,
`_death`, `_attack`); il ronzio e il tonfo li suona il mod (`core.play_sound`).

| file | prompt |
|---|---|
| `hum.ogg` | Fluorescent ceiling lights buzzing, steady electrical 60 Hz hum, empty office room, constant drone -- uscito di 1 s, ripetuto a 8 s con ffmpeg; suona sopra ogni giocatore del Level 0 ogni 8 s |
| `splash.ogg` | Person jumping into an indoor swimming pool, big splash, tiled hall echo |
| `bacteria_say.ogg` | Creepy creature clicking and wet gurgling breath, distorted, close-mic, short |
| `bacteria_hurt.ogg` | Monster pained screech, short sharp distorted shriek |
| `bacteria_death.ogg` | Creature dying, long fading distorted wail ending in a dry rattle, then a body thud on carpet |
| `bacteria_attack.ogg` | Fast whoosh of a lashing claw swipe with a raspy hiss |
| `smiler_say.ogg` | Low eerie whispering giggle in a dark concrete room, reverb, unsettling |
| `smiler_hurt.ogg` | Short distorted gasp with electrical crackle, glitchy |
| `smiler_death.ogg` | Ghostly laugh slowing down and pitching down into silence, electrical fizzle out |

## Rifare gli asset

- **Texture:** `tools/make_backrooms_textures.py` (PIL) scrive
  `textures/backrooms_*.png`.
- **Creature:** `tools/make_backrooms_creatures.py`, scatole in Blender con
  armatura e clip, esportate in `mods/backrooms/{bacteria,smiler}.gltf` e
  salvate in `assets/blender/backrooms.blend`. Da terminale:
  `flatpak run org.blender.Blender --background --python "$PWD/tools/make_backrooms_creatures.py"`
  (il percorso assoluto: il flatpak non parte dalla cartella del progetto).
  Nel Blender aperto, attraverso il server MCP: eseguire lo script senza
  `__main__` e chiamare `bacteria()` e `smiler()` dopo aver svuotato la scena
  a mano (`read_homefile` nella finestra aperta perde il contesto).
- **Pagliaccio:** nello stesso script (`clown()`), in `mods/backrooms/clown.gltf`.
- **Scivolo, scaletta e palloncini:** `tools/make_backrooms_props.py`, in
  `mods/backrooms/{slide,ladder,balloon_red,balloon_yellow,balloon_blue}.gltf` e
  `assets/blender/backrooms_props.blend`.
  La scaletta non è metallica: senza nulla da riflettere il motore la
  disegnava nera.
- **Giocatore:** il robot del sandbox, link a `mods/robot`. La clip `slide`
  è in `assets/blender/robot18.blend` (azione `slide`, dalla posa di
  `drive_idle`), esportata da sola e aggiunta a `mods/robot/robot.gltf` con
  `tools/gltf_add_clips.py`. Il client la sceglie quando il corpo è dentro uno
  scivolo (`slides` in `src/client/main.cpp`, con `chute_at`), per sé e per
  gli altri.
- **Bussole, filamento, dente:** `tools/make_held_models.py`
  (`ONLY="backrooms:compass_in,backrooms:compass_out,backrooms:filament,backrooms:smiler_tooth"`),
  modelli in mano in `mods/backrooms/*_held.gltf` e icone in
  `textures/backrooms_*.png`. La cassa è un link a `mods/chest/chest.gltf`.

## Cosa manca

- Le casse ci sono solo nelle parti delle backrooms generate da ora in poi:
  in un mondo già salvato le stanze già visitate restano senza.
- Le bussole si leggono solo cliccando un blocco a portata (il clic con una
  bussola in mano deve poter aprire una cassa); la torcia no.
- La torcia non ha un suono.
- Gli scivoli non sono solidi: a terra si passa attraverso il fianco del
  loro ultimo pezzo.
- Nessuna ringhiera attorno ai buchi e all'apertura delle scale (dentro
  l'apertura si cade al massimo di 3, sotto la soglia del danno).
- Le gallerie degli atri non hanno porte verso le stanze vicine: ci si sale
  solo dalle scale interne.
- In Xephyr il client va a 4-7 FPS e tutto è al rallentatore (camminando
  1.3 m/s invece di 4.8): lo scivolo si vede lento per lo stesso motivo.
- Si torna al sandbox solo dalle stanze all'origine del Level 0 e delle
  poolrooms.
- Il verso dello Smiler dura mezzo secondo; nelle poolrooms non c'è suono.
- I pagliacci non hanno suoni loro; arrivano in silenzio.
- I blocchi del quartiere sono colori, senza texture; le case sono tutte uguali, anche dentro (vuote).
- Il quartiere c'è solo in un mondo delle backrooms fatto alto 96.

## Crediti

Codice, blocchi, texture, modelli e animazioni: questo progetto. Suoni:
generati con ElevenLabs per questo progetto. Il robot: `mods/robot`, di
questo progetto.

## Le bussole

Due bussole, e nessuno le regala:

- **Dell'entrata** (`backrooms:compass_in`, faccia gialla): si trova per caso
  nelle casse. Una cassa (`backrooms:chest`, il modello di `mods/chest`) sta
  in una stanza vuota su quaranta circa (25 per mille delle stanze senza
  niente dentro, su tutti e tre i livelli), contro l'angolo +x +z. Si riempie
  la prima volta che qualcuno la apre, secondo un hash di dove sta: la
  bussola in due casse su cinque, la torcia in una su otto, e uno o due filamenti (Level 0), denti
  (Level 1) o l'uno o l'altro (poolrooms).
- **Dell'uscita** (`backrooms:compass_out`, faccia verde): si costruisce
  nella griglia della borsa con due **filamenti** (`backrooms:filament`) e
  due **denti dello Smiler** (`backrooms:smiler_tooth`), in qualunque ordine.
  Un Bacteria lascia un filamento una volta su due, uno Smiler un dente
  (`core.mob_drop`, 0 o 1).

## La torcia

`backrooms:torch`, rara: in una cassa su otto. Clic destro con la torcia in
mano, dovunque si guardi: un fascio di luce lungo lo sguardo, fino a 14 nodi,
stordisce per 4 s i Bacteria e gli Smiler che prende (`core.slow_mobs` al 5%:
camminano a stento e non colpiscono); poi si ricarica per 6 s. Il clic nel
vuoto non arriva al server come `core:used`, quindi lo prende il mod
`backrooms_hand` (`side = both`): la sua metà sul client reclama il clic
destro con la torcia in mano (`core:input`) e lo manda al server, che
accende la torcia e dice a tutti i client dove disegnare il fascio. Il
modello è quello della torcia del laboratorio.

## Leggere le bussole

Clic destro con una in mano, su un blocco qualunque (`core:used`), e una frase
dice dove sta l'entrata (la stanza all'origine del Level 0, davanti al
portale) o l'uscita (la stanza di `backrooms_exit` nel Level 1): metri e lato
rispetto a dove si guarda ("23 m, davanti a destra"). Da un altro livello dice
quanti livelli sopra o sotto e indica la via più vicina entro 16 stanze: scale
per salire, scale o un buco per scendere. Entrando, una frase accenna alle
casse e alle creature.

## L'uscita e la Chiave dell'Uscita

`mods/backrooms_exit`: nel Level 1, nella stanza (12, -9) -- x 96..104, z
-72..-64, oltre cento nodi dall'origine, la stessa per tutti -- c'è l'uscita
che l'ingegnere ha trovato: una stanza con una porta in ogni muro, insegne
verdi sopra le porte e, su un piedistallo al centro, la **Chiave dell'Uscita**
(`keys:backrooms`, mods/keys). Niente la indica tranne una radio: a chi è nel
Level 1 gracchia dalla direzione dell'uscita (`radio.ogg`,
`tools/make_radio_sound.py`), più spesso quanto più è vicina, e ogni 25 s un
messaggio dice se il segnale è debole, chiaro o forte. Scendere nel Level 1 è
un checkpoint: chi muore lì torna dove è sceso.

## Il livello di un sito survival

Dietro un sito survival (`keys:level` di mods/keys) le backrooms sono le stesse, ma il livello è
del sito, nel suo slot: una stanza di partenza nel Level 0 con l'uscita nascosta nel muro ovest
sotto una lampada spenta, e una stanza d'uscita come quella del mondo a 48 + 24 d, nel Level 0
fino a d 3 e nel buio del Level 1 da d 4. La radio porta lì; arrivati, una porta verde riporta
alla stanza di partenza, dove il muro si è aperto. Da d 5 le luci attorno al giocatore si
spengono ogni tanto per otto secondi.
