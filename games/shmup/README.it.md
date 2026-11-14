# Lo sparatutto da sala giochi

*[Read in English](README.md)*

Uno sparatutto visto dall'alto su un caccia, come nei cabinati degli anni ottanta. **Una prima
imbastitura**: uno schermo che non scorre, ondate di aerei, tre vite, un punteggio e un record. Due
mod: `mods/shmup_core` (solo server: il mare, il ponte della portaerei, i muri dello schermo, gli aerei, e il
corpo -- un caccia, che vola) e `mods/shmup` (`side = both`: la partita sul server; la
telecamera, i colpi e le scritte sulla macchina del giocatore).

## Come ci si arriva

Nel gioco (il menù del titolo, o `tools/track.sh`) il suo cancello è sul lato sud della piazza, a
56.5 21 26.5: un cabinato nero da sala giochi (`mods/gates`). Il mondo non parte col server: sta
nel file delle stanze, si avvia quando qualcuno si avvicina al cancello e si ferma un minuto dopo
che l'ultimo se n'è andato; il cancello è un portale fra stanze che prende in prestito una bocca.
`tools/shmup.sh` avvia il sandbox con questo mondo soltanto, dietro la prima apertura
(`tools/shmup.sh -- --room shmup` ci nasce dentro). In tutti e due i casi si esce sul ponte di una portaerei in fondo
all'immagine, già caccia e già visti dall'alto (`core.top_view_at`, nuova con questo gioco:
`docs/MODDING.it.md`). La porta a poppa, verso il basso dello schermo, riporta indietro.

## La partita

| tasto | |
|---|---|
| spazio | fa partire la partita; tenuto, spara |
| W, A, S, D | muovono il caccia su, a sinistra, giù, a destra: guarda sempre verso l'alto |
| E | torna sul ponte (fuori da questa partita) |

I dardi, verde oliva, scendono dritti (100 punti). I serpeggianti, arancioni, oscillano da un lato all'altro e ogni tanto
mirano un colpo (150). Gli assi, neri con le fasce rosse, si fermano a metà e ne sparano tre alla volta (400). Ogni quinta
ondata è un bombardiere grigio che regge quaranta colpi (3000). Un colpo o un aereo che ti tocca costa una
vita, e per due secondi e mezzo dopo niente ti tocca (sulle punte delle ali lampeggia l'azzurro). Il tuo caccia è quello argento con le punte delle ali rosse.
Finite le vite si torna sul ponte; quando ci sono tutti la partita è finita e il record resta
(`core.storage_set`). Più giocatori volano le stesse ondate, ognuno con le sue vite e il suo
punteggio; chi arriva a partita iniziata entra con la barra spaziatrice.

## Com'è fatto

Il server tiene tutto quello che decide: dove sono aerei e colpi, chi è stato colpito. Alla
macchina del giocatore ogni colpo viene detto una volta, quando parte, e lei lo fa volare da sola
finché non le si dice che è sparito. Gli aerei sono entità del mod (`core.ecs.visible`); i modelli sono l'aereo e il bombardiere del fronte con
livree loro (`tools/make_shmup_models.py`, Blender), le immagini e i suoni sono di
`tools/make_shmup_assets.py`. Il mare
sta fermo: le strisce di schiuma e le nuvole che passano le disegna la macchina del giocatore
(`core.render.pass`). L'alto dell'immagine è +z, quindi la destra è -x.

## Quello che manca

Vedi `TODO.md`, "Platform game and arcade shooter (drafts)".
