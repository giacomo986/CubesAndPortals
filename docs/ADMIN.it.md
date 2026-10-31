# Gestire un server

*[Read in English](ADMIN.md)*

Tutto quello che un proprietario o un membro dello staff può fare a un `cnp_server`: i comandi,
chi li può usare, i flag e le chiavi di `server.conf`, gli script, e cosa tiene su disco ognuno.
Il codice è [`src/server/admin.h`](../src/server/admin.h) e `admin.cpp`; ogni comando passa da
una funzione sola lì, in qualunque modo sia stato scritto.

## Indice

- [Da dove arrivano i comandi](#da-dove-arrivano-i-comandi)
- [Ruoli e permessi](#ruoli-e-permessi)
- [Comandi](#comandi)
- [Pagine nel gioco](#pagine-nel-gioco)
- [Flag e server.conf](#flag-e-serverconf)
- [Tenerlo acceso](#tenerlo-acceso)
- [Riavvii, arresti e manutenzione](#riavvii-arresti-e-manutenzione)
- [Cosa trattiene il mute](#cosa-trattiene-il-mute)
- [Regole da accettare](#regole-da-accettare)
- [Account alternativi](#account-alternativi)
- [Controllo degli aggiornamenti](#controllo-degli-aggiornamenti)
- [Visite, log e rapporti](#visite-log-e-rapporti)
- [Blocchi: ispezione e rollback](#blocchi-ispezione-e-rollback)
- [Backup](#backup)
- [Webhook](#webhook)
- [Portali fra server](#portali-fra-server)
- [Server amici](#server-amici)
- [Cosa si tiene, e dove](#cosa-si-tiene-e-dove)
- [Limiti](#limiti)

## Da dove arrivano i comandi

| Via | Come | Ruolo |
|---|---|---|
| Chat | `/ban bob 2h griefing` scritto nel gioco | quello del giocatore |
| Console | lo stdin del server, senza la barra: `ban bob 2h griefing` | sopra ogni ruolo |
| RCON | `--rcon PORTA --rcon-password PW`, TCP solo su 127.0.0.1; `CNP_RCON_PASSWORD=PW tools/admin.sh -p PORTA ban bob 2h` | sopra ogni ruolo |
| Pagine | `/admin` nel gioco, pulsanti che eseguono gli stessi comandi | quello del giocatore |

Il protocollo RCON è fatto di righe semplici: prima la password (risposta `ok`), poi un comando
a riga, ognuno risposto con le sue righe e una riga con un `.` da solo. La password va in
`server.conf`, non sulla riga di comando, dove `ps` la mostra.

`/help` elenca i comandi che chi lo chiede può usare. Un mod non può dichiarare un comando con
il nome di un comando di amministrazione, né `page` o `admin`.

## Ruoli e permessi

`guest < player < helper < moderator < admin < owner`.

- **guest**: un giocatore senza identità (senza chiave). A un guest non si può dare niente,
  perché un nome senza chiave è un'etichetta che chiunque può scrivere.
- **player**: chiunque abbia una chiave, se non gli è stato dato di più.
- **owner**: `--owner NOME` (il client lo passa per i mondi che avvia da sé), oppure
  `/role NOME owner` dalla console.

I ruoli si danno per chiave: un giocatore tiene il suo anche sotto un altro nome. Nessuno dà un
ruolo uguale o superiore al proprio, e nessuno agisce su un giocatore dello stesso ruolo o di uno
superiore (la console sì).

`/role NOME RUOLO STANZA` dà un ruolo in una stanza sola: in quella stanza il giocatore ha il più
alto dei due. Un moderatore dell'arena è un giocatore nel sandbox.

Ogni comando chiede un permesso, e ogni permesso vuole un ruolo minimo. I valori di partenza:

| Ruolo | Permessi |
|---|---|
| helper | `mute`, `pages`, `staffchat`, `warn`, `reports`, `notes`, `players`, `inspect` |
| moderator | `kick`, `ban`, `creative`, `announce`, `whois`, `rollback`, `bag`, `alts` |
| admin | `ban.ip`, `access`, `roles`, `restart`, `maintenance`, `motd`, `rules`, `logs`, `backup`, `portals`, `give` |
| owner | `perms`, `federation` |

`/perm PERMESSO RUOLO` (owner) sposta un permesso a un altro ruolo; `/perm` li elenca.
`/perm PERMESSO nobody` lo chiude a tutti, anche al proprietario (sopra c'è solo la console).
Un mondo survival (`--survival`) parte con `creative` a `nobody`: niente catalogo (C) per
nessuno; `/perm creative owner` (dalla console) lo ridà.
Anche il nome di un ruolo è un permesso (`moderator`), per un mod che non ha una parola migliore.

Mod: `core.command(nome, aiuto, permesso)` protegge un comando di un mod; `core.has_perm(giocatore,
permesso)` e `core.role(giocatore)` chiedono (vedi [`API.md`](API.md)).

## Comandi

Le durate sono `45s`, `30m`, `2h`, `7d`, `1w`. NOME è un giocatore online in qualunque stanza, o
il proprietario di quel nome in questo mondo, o il nome e basta.

### Chi può entrare

| Comando | Permesso | Cosa fa |
|---|---|---|
| `/perms` | chiunque | Il tuo ruolo qui |
| `/role [NOME [RUOLO [STANZA]]]` | roles | Elenca i ruoli dati; mostra quello di un giocatore; ne dà uno (in una stanza con STANZA). `player` lo toglie |
| `/kick NOME [motivo]` | kick | Fuori da ogni stanza, anche dalle finestre dei portali; il client torna al titolo dicendo perché |
| `/ban NOME [tempo] [motivo]` | ban | Fuori e tenuto fuori, per un tempo o per sempre. Un nome con un proprietario bandisce la chiave del proprietario; un nome senza bandisce anche l'indirizzo da cui è arrivato l'ultima volta |
| `/banip NOME\|INDIRIZZO [tempo] [motivo]` | ban.ip | Tiene fuori un indirizzo |
| `/unban NOME\|INDIRIZZO` | ban | Toglie un ban. Togliere qui il ban di un server amico lo tiene tolto |
| `/bans` | ban | Chi è bandito, da chi, fino a quando |
| `/mute NOME [tempo] [motivo]` `/unmute NOME` | mute | Trattiene la chat di un giocatore, e le sue parole dovunque un mod le riporti (sotto). Un nome che nessuno possiede ancora è silenziato per nome, e il mute resta quando arriva con una chiave |
| `/whitelist on\|off\|list\|add NOME\|remove NOME` | access | Entrano solo i giocatori in elenco, e lo staff |
| `/perm [PERMESSO RUOLO]` | perms | Quale ruolo vuole ogni permesso |
| `/warn NOME motivo` | warn | Un avvertimento; tre in 30 giorni sono un giorno di ban |
| `/rules [set TESTO\|off]` | chiunque (rules per cambiarle) | Le regole che tutti accettano; impostarne di nuove le richiede a tutti |
| `/accept` `/leave` | chiunque | Accetta le regole, o lascia il server senza accettarle |

### La vita del server

| Comando | Permesso | Cosa fa |
|---|---|---|
| `/restart [5m\|now\|cancel] [motivo]` | restart | Riavvio con avvisi su ogni schermo (10m, 5m, 1m, 30s, 10s e gli ultimi secondi). Il server esce con lo stato 75 per `tools/serve.sh`; i client messi fuori come "restarting" rientrano da soli |
| `/stop [5m\|now\|cancel] [motivo]` | restart | Lo stesso, e il server resta fermo |
| `/maintenance on [testo]\|off` | maintenance | Entra solo lo staff; gli altri sono messi fuori con il testo. La lista dei server nel client mostra "maintenance" |
| `/motd [TESTO\|off]` | motd (per cambiarlo) | Il messaggio del giorno, detto in chat a ogni giocatore che entra |
| `/announce TESTO` | announce | Un avviso su ogni schermo |
| `/backup` | backup | Copia ora ogni file di mondo |
| `/metrics` | logs | Da quanto è acceso, giocatori e finestre per stanza, il vuoto più lungo fra due tick |
| `/weather [clear\|rain\|storm] [MINUTI]` | admin | Il cielo del sandbox (`mods/weather`): senza parola, com'è e quanto dura; con una, quel cielo adesso, per MINUTI o il tempo solito |
| `/season [spring\|summer\|autumn\|winter]` | admin | La stagione del sandbox (`mods/weather`, cinque giorni l'una): senza parola, quale e che giorno; con una, il primo giorno di quella stagione quest'anno |
| `/village siege\|feast\|caravan\|incoming` | admin | Avvia subito uno degli eventi dei villaggi nel villaggio dove sta l'admin (`mods/villagers`): un assedio annunciato ora che arriva al calar della notte, la festa del raccolto nella sua piazza, una carovana verso l'altro villaggio più vicino (nessuna se ce n'è già una in giro o i suoi bauli non hanno niente da dare), o una che arriva da un villaggio a 100-900 che nessuno ha visitato, lungo i sentieri (nessuna se nessun villaggio così è collegato) |

### Giocatori

| Comando | Permesso | Cosa fa |
|---|---|---|
| `/seen NOME` | chiunque | Quando un giocatore è stato qui l'ultima volta, e per quanto in tutto |
| `/players [7d]` | players | Chi è entrato di recente, visite e tempo giocato |
| `/whois NOME` | whois | Chiave, nomi usati, indirizzi, avvertimenti, note, ban |
| `/alts NOME` | alts | Altri giocatori entrati da un indirizzo da cui è entrato questo, e quali sono banditi |
| `/bag NOME` | bag | Cosa porta un giocatore, casella per casella, e cosa tengono i suoi zaini; offline, la borsa tenuta qui; e se è andata a un server amico |
| `/give NOME OGGETTO [QUANTI]` | give | Oggetti per nome (`chest:box`) nella borsa di un giocatore online |
| `/report NOME cosa è successo` | chiunque | Un rapporto per lo staff, detto subito a chi è online |
| `/reports` | reports | Gli ultimi rapporti |
| `/note NOME testo` | notes | Una nota su un giocatore, mostrata in `/whois` |
| `/sc TESTO` | staffchat | Chat fra lo staff online |
| `/logs [N] [AZIONE]` | logs | Le ultime azioni di amministrazione, tutte o di un tipo (`ban`, `rollback`...) |

### Blocchi

| Comando | Permesso | Cosa fa |
|---|---|---|
| `/inspect X Y Z [STANZA]` | inspect | Gli ultimi dieci giocatori che hanno cambiato quel blocco, da cosa a cosa, quando |
| `/rollback NOME DURATA [STANZA]` | rollback | Rimette ogni blocco che quel giocatore ha cambiato in quel tempo |

### Portali e server amici

| Comando | Permesso | Cosa fa |
|---|---|---|
| `/portals` | portals | Dove portano le aperture di ogni stanza, e quali server mandano giocatori, quanti, da dove |
| `/friend add NOME HOST:PORTA CHIAVE [cosa]` | federation | Un server amico; `cosa` è uno qualunque di `bans,presence,chat,bags`, tutti se non si dice |
| `/friend remove NOME` `/friend list` | federation | Smettere di essere amici (i suoi ban qui si tolgono); gli amici, e quando si è sentito ognuno l'ultima volta |
| `/fed TESTO` | chiunque | Chat con i giocatori di ogni server amico |
| `/where NOME` | chiunque | Qui (e in quale stanza), o su quale server amico |

### Pagine

| Comando | Permesso | Cosa fa |
|---|---|---|
| `/page [NOME]` | chiunque | Le pagine nel gioco, o una di esse |
| `/admin` | pages | La pagina di amministrazione |

## Pagine nel gioco

`/admin` apre una pagina con chi è online e i pulsanti per kick, ban e mute; da lì `bans`,
`whitelist`, `roles`, `perms`, `running` (riavvii, manutenzione, messaggio del giorno, backup),
`players`, `reports` e `log`. Tutti hanno `server` (le stanze e chi c'è), `mods`, `here` (dove sei
e di chi è il terreno), `rules` e le pagine che aggiungono i mod. Una pagina mostra solo quello che
chi la guarda può fare, e ogni pulsante esegue il comando che scriverlo eseguirebbe.

Le pagine le impagina il server in inglese. Un client che traduce un'etichetta o un pulsante
disegna la traduzione più piccola quando è più larga dello spazio che ha, fino a due terzi della
dimensione, e oltre le parole inglesi, che per costruzione ci stanno.

## Flag e server.conf

`--config FILE` legge i flag da un file nel punto in cui sta sulla riga di comando, quindi i flag
dopo vincono. Una riga `chiave valore` o `chiave = valore`, che vuol dire `--chiave valore`;
`chiave = true` è un flag senza valore; `#` comincia un commento; `[room NOME]` comincia i flag di
una stanza.

```
# server.conf
listen = 25300
world-file = worlds/main.world
owner = alice
rcon = 25399
rcon-password = change-me
motd = Welcome! /help lists the commands.
rules-file = rules.txt
restart-at = 04:00
backup-every = 6h
backup-keep = 8
max-per-address = 3
watchdog = 60
federation = 25480
webhook = https://example.org/hooks/server
```

| Flag | Cosa fa |
|---|---|
| `--owner NOME` | Chi gioca sotto NOME con una chiave è il proprietario |
| `--rcon PORTA` `--rcon-password PW` | Comandi di amministrazione via TCP su 127.0.0.1 |
| `--motd TESTO` | Il messaggio del giorno |
| `--rules-file FILE` | Le regole da accettare, da un file (più righe); vedi sotto |
| `--no-update-check` | Non chiedere a GitHub se è uscita una release più nuova |
| `--restart-at HH:MM` | Un riavvio ogni giorno a quell'ora locale, con dieci minuti di avvisi |
| `--backup-every 6h` `--backup-keep N` | Backup mentre gira; restano gli ultimi N (5) |
| `--max-per-address N` | Al massimo N giocatori da un indirizzo |
| `--watchdog S` | Una stanza bloccata per S secondi ferma il server stampando lo stack |
| `--federation PORTA` | La porta TCP dei server amici, sotto la chiave dei ticket (`--ticket-key`, o WORLD.sign) |
| `--webhook URL` | Azioni di amministrazione, avvio, arresto e rapporti mandati lì con POST |
| `--max-players N` `--require-identity` `--no-observers` | Posti; i giocatori devono avere una chiave; niente finestre dei portali da altri mondi |
| `--report FILE` `--since 7d` `--json` | Senza server: cosa ricorda un file di mondo, stampato; `--json` come un oggetto JSON |
| `--names` `--free-name NOME` | Senza server: chi possiede ogni nome; liberarne uno |
| `--survival` | Di una stanza (il sandbox): niente piazza, lo spawn su terra asciutta scelta dal seme, i mondi a tema dietro portali sparsi sulla terra; `creative` parte a `nobody`. "Mode: Survival" nel menù del titolo scrive `survival` nel file `mode` del mondo e lo passa; anche `tools/track.sh --survival` |

Gli altri flag (mondo, mod, portali, stanze) sono nel [README](../README.it.md#server-cnp_server).

## Tenerlo acceso

```
tools/serve.sh -l logs -k 14 -w https://example.org/hooks/server \
    build-release/src/server/cnp_server --config server.conf
```

- Riavvia il server quando si riavvia (stato d'uscita 75: `/restart`, `--restart-at`) o va in
  crash (compreso un arresto del `--watchdog`), cinque secondi dopo un crash.
- Lo lascia fermo dopo `/stop`, Ctrl+C o SIGTERM (stato d'uscita 0), e rinuncia dopo cinque crash
  in dieci minuti.
- Scrive tutto quello che il server stampa in `LOGDIR/server-AAAA-MM-GG.log`, un file al giorno,
  l'ora su ogni riga; i file più vecchi di `-k` giorni (14) se ne vanno.
- `-w URL` manda con POST ogni crash e la rinuncia, con gli stessi campi di `--webhook`.
- Il suo stdin è quello del server, quindi la console funziona anche sotto.

## Riavvii, arresti e manutenzione

Un riavvio o un arresto conta alla rovescia con un avviso su ogni schermo, poi mette fuori tutti
con il motivo "restarting" o "stopping" e salva. Un client messo fuori da un riavvio torna al
titolo e rientra da solo nello stesso server appena risponde (per tre minuti). `/restart cancel`
ferma il conto alla rovescia.

La manutenzione tiene fuori tutti tranne lo staff; chi è dentro e non è staff viene messo fuori
con il suo testo. Sopravvive a un riavvio, fino a `/maintenance off`.

## Cosa trattiene il mute

La chat di un giocatore silenziato è trattenuta, e così quello che un mod dice agli altri mentre
esegue un suo comando, un'azione di pagina, un clic su un pannello o un messaggio. E le parole che
ha dato a un mod si annotano: una riga di chat, un avviso o una riga di pagina di un mod che porta
tre di quelle parole di fila (dodici lettere o più, o tutto quello che ha detto se è meno) arriva
solo a lui, allora o dopo -- una lettera consegnata a un tick successivo, un biglietto appeso a una
bacheca. Una riga di pagina trattenuta si legge `(held: NOME is muted)`. Le parole si annotano
mentre il giocatore è silenziato e si tengono finché resta silenziato e per un'ora dopo; quello che
ha detto in meno di sei lettere no. Il log dice ogni volta che una riga è trattenuta.

## Regole da accettare

Con `--rules-file FILE` (o `/rules set TESTO`), ogni giocatore che entra vede la pagina `rules`
con il testo e due pulsanti, Accept e Leave. Finché non accetta non può scrivere in chat, eseguire
comandi diversi da `/rules`, `/accept`, `/leave` e `/help`, né rompere o piazzare blocchi; la
pagina torna ogni volta che ci prova. Lo staff si considera d'accordo. Impostare regole diverse le
richiede a tutti; lo stesso testo riletto a un riavvio no. `/rules off` le toglie.

## Account alternativi

`/alts NOME` elenca gli altri giocatori (chiavi) entrati da uno qualunque degli indirizzi da cui è
entrato quel giocatore, e quali di loro sono banditi. Quando un giocatore entra da un indirizzo
usato da un giocatore bandito, lo staff online lo sa subito (`[alts] bob came in from where eve
did, who is banned`). Un indirizzo non è una persona: una famiglia, una scuola o una rete mobile ne
condividono uno.

## Controllo degli aggiornamenti

Un server in ascolto chiede a GitHub una volta, all'avvio, se è uscita una release più nuova della
sua (la stessa domanda che fa la schermata del titolo del client). Quando c'è, il log lo dice, e
ogni admin online e ognuno che entra lo sa in chat. `--no-update-check` lo spegne.

## Visite, log e rapporti

Ogni visita è una riga: chi, da quale indirizzo, in quale stanza, da quando a quando, e perché è
finita. Un crash lascia aperte le visite; l'avvio successivo le chiude e lo scrive come un arresto
sporco. Ogni azione di amministrazione va nel log di amministrazione con chi l'ha fatta, a chi, e i
dettagli.

`cnp_server --report worlds/main.world --since 7d` stampa, senza fermare il server: giocatori e il
loro tempo, il massimo contemporaneo, ingressi per ora, stanze, chi è stato respinto e perché,
azioni dello staff, riavvii e crash, e arrivi attraverso portali da altri server. Con `--json` gli
stessi numeri arrivano come un oggetto JSON (`players`, `visits`, `seconds_played`,
`most_at_once`, `joins_by_hour`, `most_played`, `rooms`, `refused`, `staff_actions`, `server`,
`crossings_in`), per un programma.

## Blocchi: ispezione e rollback

Ogni blocco che un giocatore rompe o piazza si annota: la stanza, la cella, cosa c'era, cosa è
diventato, chi e quando. Vanno nel file insieme ogni cinque secondi; si tiene un mese.

`/rollback NOME 2h` rimette ogni cella che quel giocatore ha cambiato nelle ultime due ore com'era
prima del suo primo cambio in quel tempo, a meno che qualcuno l'abbia cambiata dopo di lui: quel
cambio resta. Quelle righe poi non ci sono più, quindi un secondo rollback non fa niente. Quello
che il giocatore ha preso dai blocchi resta nella sua borsa; per quello ci sono `/bag` e i comandi
degli oggetti.

## Backup

`--backup-every 6h` e `/backup` copiano ogni file di mondo accanto a sé come
`NOME.backup-AAAAMMGG-HHMMSS`, con il backup online di SQLite su un thread suo, mentre il server
gira. Restano gli ultimi `--backup-keep`.

Per tornare a uno: fermare il server, mettere da parte il file attuale, copiare il backup sopra il
file di mondo (i file `.key`, `.sign` e `.lock` accanto restano come sono), e riavviarlo.

## Webhook

Con `--webhook URL`, ogni azione di amministrazione, ogni `/report`, e l'avvio del server
(`unclean start` dopo un crash) e il suo arresto si mandano con POST come JSON:

```json
{"event":"ban","by":"alice","target":"bob","detail":"until 2026-10-28 18:00, griefing",
 "at":1793120997,"content":"[ban] alice bob: until 2026-10-28 18:00, griefing",
 "text":"[ban] alice bob: until 2026-10-28 18:00, griefing"}
```

`content` e `text` sono la stessa frase, che i webhook in ingresso dei servizi di chat mostrano
così com'è. Il POST passa da `curl`, cinque secondi al massimo, su un thread suo: un webhook giù
non costa niente. L'URL deve essere `http://` o `https://` con caratteri semplici.

## Portali fra server

`--portal-to HOST:PORTA[/N]` dice dove porta un'apertura gestita dal server; `--trust HEX` accetta
i ticket che firma un altro server, `--require-ticket` non accetta nessun altro. Un client che
attraversa controlla che la chiave del server lontano sia quella che il collegamento nomina,
attraverso una finestra di portale come attraverso il redirect lento. `/portals` mostra le due
direzioni.

## Server amici

Due server che si aggiungono a vicenda condividono, sulla loro porta `--federation`:

- **ban**: i ban permanenti per chiave. Il ban di un amico vale qui finché `/unban` non lo toglie
  (e poi resta tolto) o l'amico non viene rimosso.
- **presenza**: chi è online, per `/where`.
- **chat**: `/fed`.
- **borse**: una borsa per giocatore su tutti (sotto).

Per fare amici due server:

1. Avviare ognuno con `--federation PORTA`; il log dice `this server's key HEX`, e `/friend list`
   lo ripete.
2. Sul primo: `/friend add second second.example.org:25480 HEX_DEL_SECONDO`.
3. Sul secondo: `/friend add first first.example.org:25480 HEX_DEL_PRIMO`.

Ogni riga viaggia su una connessione TCP sua. Parla prima chi riceve, con una chiave fatta solo per
quella connessione; chi manda risponde con una sua e la riga, sigillata per chi riceve
(XChaCha20-Poly1305, con una chiave dalle chiavi a lungo termine dei due server e dalle due usa e
getta) e firmata con la chiave dei ticket di chi manda. Le chiavi usa e getta si dimenticano
subito, quindi una riga registrata oggi non si apre domani nemmeno con le chiavi di tutti e due i
server: forward secrecy. Una riga più vecchia dell'ultima di chi la manda, lontana cinque minuti da
questo orologio, o da una chiave che non è in elenco, si scarta.

### Una borsa sola

La borsa di un giocatore vive su un server amico alla volta e va dove va lui:

- **Attraverso un portale** da un amico: il server dove arriva chiede a quello da cui viene, che la
  lascia (il giocatore lì è ora una finestra) e la manda.
- **Entrando** dal menù: il server chiede a ogni amico chi ha la copia più nuova. La più nuova, se
  più nuova della sua, la manda e la lascia. Più nuova è la copia passata più volte fra amici, che
  non ha bisogno di un orologio (solo chi la tiene riceve un conto più alto); due copie mai mosse,
  di prima che i server fossero amici, si distinguono da quando sono state tenute.
- Chi l'ha mandata ricorda dove è andata: una visita successiva lì chiede prima a quel server, e
  `/bag` lì lo dice.
- Mentre arriva (pochi millisecondi, al massimo cinque secondi) il giocatore ha una borsa vuota, e
  niente salva sopra quella su disco. Un amico che non risponde in tempo lascia il giocatore con la
  borsa tenuta qui; se la borsa dell'amico arriva dopo lo stesso, prende il posto di quella copia
  vecchia, e quello che il giocatore ha raccolto intanto ci finisce dentro.
- **Gli zaini** (`mods/backpack`) vanno con il loro contenuto. Il contenuto di uno zaino si tiene
  sotto il suo numero, che il mod di ogni server conta da 1, quindi il server dove arriva uno zaino
  gliene dà uno suo da 100000 in su (tenuto quando torna su un server dove quel numero è libero).
- **Le cose che un mondo non conosce** (un oggetto di un mod che non ha) si nascondono: tenute da
  parte, fuori dalla borsa, e mandate con lei, uno zaino nascosto con il suo contenuto; tornano su
  un server che le conosce, nella casella che avevano (uno zaino sulla schiena) o in una libera.
  `/bag` dice quando un giocatore ne ha.

## Cosa si tiene, e dove

Nel file di mondo della prima stanza (un archivio per tutte le stanze), accanto al mondo:

| Tabella | Cosa |
|---|---|
| `admin` | Una riga per cosa: `role`, `ban`, `mute`, `allow` (whitelist), `setting` (whitelist accesa, manutenzione, messaggio del giorno, regole), `accepted` (quali regole ha accettato un giocatore), `perm`, `addr` (ultimo indirizzo), `friend`, `fedskip` (il ban di un amico tolto qui), `bagaway` (la borsa di chi è andata a quale amico), `baggen` (quante volte una borsa si è mossa), `hidden` e `packs` (quello che un giocatore porta che questo mondo non conosce, e cosa tengono i suoi zaini) |
| `admin_log` | Ogni azione di amministrazione, avvio, arresto, arresto sporco, rapporto, nota, avvertimento |
| `sessions` | Una riga per visita |
| `edits` | Ogni blocco cambiato da un giocatore, un mese |

Le borse stesse stanno nel file di mondo di ogni stanza, come sempre.

## Limiti

- `/mute` riconosce le parole di un giocatore silenziato da tre di fila: un mod che le riformula, o
  una riga sotto le sei lettere, passa; un altro giocatore che ripete la sua frase entro l'ora è
  trattenuto anche lui. Un mod che riporta parole chiede `core.is_muted` da sé, come fanno `/msg` e
  `/me`.
- Un amico che resta irraggiungibile mentre un giocatore continua con la copia tenuta qui tiene
  anche la sua; vince quella che si è mossa più volte la prossima volta che le due si confrontano.
- Il controllo del client che la chiave del server lontano sia quella che nomina un portale sta
  dietro il ticket: un server che non è quello nominato rifiuta il ticket prima che il client legga
  la sua chiave, quindi il controllo conta solo per un server che mentirebbe anche sui ticket.
