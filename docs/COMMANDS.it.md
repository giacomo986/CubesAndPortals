# Comandi

*[Read in English](COMMANDS.md)*

Ogni comando che un giocatore può scrivere in chat (T, poi `/nome`), cos'è e cosa fa.
`/help` nel gioco elenca quelli che puoi usare dove sei; l'elenco cambia con il mondo,
perché la maggior parte dei comandi viene dai suoi mod.

La console del server accetta anche i comandi di amministrazione, con o senza `/`, come
proprietario. Le durate sono `45s`, `30m`, `2h`, `7d`, `1w`. NOME è un giocatore online in
una qualsiasi stanza, il proprietario di quel nome su questo mondo, o il nome da solo.

**Permesso** è chi può usarlo. "Chiunque" non chiede nulla; gli altri indicano un permesso
e il ruolo che lo ha di default (helper < moderator < admin < owner, ognuno con tutto
quello sotto). `/perm` mostra e cambia quale ruolo chiede un permesso;
[`ADMIN.it.md`](ADMIN.it.md) spiega i ruoli.

## Per tutti

### Chat e informazioni

| Comando | Permesso | Cosa fa |
|---|---|---|
| `/help` | chiunque | I comandi che puoi usare in questo mondo, uno per riga |
| `/list` | chiunque | Chi c'è in questo mondo |
| `/me <azione>` | chiunque | Dice `* nome azione` a tutti (`mods/whisper`) |
| `/msg <giocatore> <testo>` | chiunque | Manda il testo solo a quel giocatore (`mods/whisper`) |
| `/seen NOME` | chiunque | Quando quel giocatore è stato qui l'ultima volta, e quanto ha giocato in tutto |
| `/where NOME` | chiunque | Se quel giocatore è qui e in quale stanza, o su quale server amico |
| `/fed TESTO` | chiunque | Chat con i giocatori di tutti i server amici |
| `/report NOME cosa è successo` | chiunque | Una segnalazione per lo staff, detta subito a chi è online |
| `/perms` | chiunque | Il tuo ruolo qui |
| `/rules` | chiunque | Le regole del server |
| `/accept` | chiunque | Accetta le regole del server, che un server che ne ha chiede a tutti |
| `/leave` | chiunque | Lascia il server senza accettare le regole |
| `/page [NOME]` | chiunque | Le pagine nel gioco: senza nome l'elenco (`server`, `mods`, `here`, `rules` e quelle dei mod), con un nome quella pagina |

### Giocare

| Comando | Permesso | Cosa fa |
|---|---|---|
| `/kill` | chiunque | Muori e torni al tuo letto o allo spawn del mondo; l'armatura si consuma un poco, come per un colpo qualsiasi. Per un corpo bloccato da qualche parte. Funziona anche in modalità creativa (`mods/creative`) |
| `/giveup` | chiunque, in un mondo con una prova | Abbandona la prova di quel mondo: muori, poi vieni fatto uscire dall'ingresso del mondo e torni nel sandbox, fuori dal portale di quel mondo. Un corpo che non può essere ferito viene solo fatto uscire. Quello che la prova ha già contato (una chiave guadagnata) resta (`mods/keys`) |
| `/journal` | chiunque | Apre le pagine dell'ingegnere delle chiavi che porti, una pagina per chiave e un'ultima quando le hai tutte e sette (`mods/keys`) |
| `/town [città]` | chiunque | Ti porta nella piazza del villaggio più vicino; con una parola qualsiasi dopo, della città più vicina (un villaggio più grande, cercata al doppio della distanza) (`mods/villages`) |
| `/creative TESTO` | creative (moderator) | Apre il catalogo creativo ristretto agli oggetti il cui nome contiene il testo (`mods/creative`) |

### In un solo mondo

| Comando | Mondo | Permesso | Cosa fa |
|---|---|---|---|
| `/garage` | pista | chiunque | Apre il garage: i colori e i pezzi del tuo kart; un clic lo cambia subito, anche in pista, e resta |
| `/pieces` | pista | chiunque | Ti mette nella borsa una pila di ogni pezzo di pista, l'editor della pista (B con lui in mano sceglie da lontano il pezzo dopo), e gli strumenti linea di partenza, checkpoint, fuoripista e scatola degli oggetti |
| `/vehicle <jeep\|tank\|heli\|plane\|bomber\|nest\|mortar\|howitzer\|aa>` | fronte, assalto | moderator | Mette quel mezzo sei nodi davanti a te, per le prove |

## Per lo staff

### Mondo e tempo

| Comando | Permesso | Cosa fa |
|---|---|---|
| `/kill NOME` | moderator | Uccide un altro giocatore, che torna al suo letto o allo spawn (`mods/creative`) |
| `/creative on\|off` | admin | La modalità creativa per te: volo senza jetpack, nessun danno, scavo dieci volte più veloce; tenuta per nome tra un riavvio e l'altro. Libera per il proprietario di un mondo aperto dal menù del titolo (`mods/creative`) |
| `/give NOME OGGETTO [QUANTI]` | give (admin) | Mette oggetti, per nome (`chest:box`), nella borsa di un giocatore online |
| `/weather [clear\|rain\|storm] [MINUTI]` | admin | Il cielo del sandbox: senza parola, com'è e quanto dura; con una, quel cielo subito, per MINUTI o il suo tempo solito (`mods/weather`) |
| `/time [day\|noon\|dusk\|night\|midnight\|dawn\|HH:MM]` | admin | L'orologio: senza parola, l'ora; con una, quell'ora subito per tutti. Portato indietro oltre la mezzanotte conta un giorno, come una notte dormita (`mods/weather`) |
| `/season [spring\|summer\|autumn\|winter]` | admin | La stagione del sandbox (cinque giorni l'una): senza parola, quale e che giorno; con una, il primo giorno di quella stagione (`mods/weather`) |
| `/difficulty [peaceful\|easy\|normal\|hard]` | admin | Quanto è difficile il mondo, subito e dopo i riavvii (WORLD.difficulty): pacifico, nessuna creatura che combatte esce o resta -- lo spawner di un dungeon lascia quello che lascerebbero le sue creature, l'altare del profondo dà cuore e frammenti senza lotta, i villaggi non hanno assedi; facile dimezza il colpo di una creatura, quante ne escono e la fame; difficile aggiunge metà a entrambi. Senza parola, com'è (`mods/rules`) |
| `/peaceful [on\|off]` | admin | `/difficulty peaceful`, o di nuovo normale (`mods/rules`) |
| `/rule [NOME on\|off]` | admin | Le regole del mondo, dopo i riavvii (WORLD.rules): `keep_bag` (in un mondo survival la borsa resta alla morte, niente tomba), `day_cycle` (il sole si muove), `weather_cycle` (il meteo cambia da solo), `hunger` (fame). Senza parola, l'elenco (`mods/rules`) |
| `/goals` | chiunque | Quanti obiettivi del sandbox hai raggiunto; `/page goals` li elenca con un suggerimento per i prossimi (`mods/goals`) |
| `/village siege\|feast\|caravan\|incoming` | admin | Fa partire subito un evento nel villaggio in cui sei: un assedio annunciato ora e in arrivo al tramonto, la festa del raccolto, una carovana verso il villaggio più vicino, o una in arrivo da un villaggio mai visitato lungo i sentieri (`mods/villagers`) |

### Giocatori

| Comando | Permesso | Cosa fa |
|---|---|---|
| `/role [NOME [RUOLO [STANZA]]]` | roles (admin) | Elenca i ruoli dati; mostra quello di un giocatore; ne dà uno (in una stanza con STANZA). `player` lo toglie |
| `/kick NOME [motivo]` | kick (moderator) | Mette il giocatore fuori da ogni stanza, finestre dei portali comprese; il suo client torna al titolo dicendo perché |
| `/ban NOME [tempo] [motivo]` | ban (moderator) | Fuori e tenuto fuori, per un tempo o per sempre. Un nome con un proprietario banna la chiave del proprietario; un nome senza banna anche l'indirizzo da cui è venuto l'ultima volta |
| `/banip NOME\|INDIRIZZO [tempo] [motivo]` | ban.ip (admin) | Tiene fuori un indirizzo |
| `/unban NOME\|INDIRIZZO` | ban (moderator) | Toglie un ban; toglierne qui uno di un server amico lo tiene tolto |
| `/bans` | ban (moderator) | Chi è bannato, da chi, fino a quando |
| `/mute NOME [tempo] [motivo]` | mute (helper) | Trattiene la chat del giocatore, e le sue parole ovunque un mod le riporti. Un nome che nessuno possiede ancora è silenziato per nome |
| `/unmute NOME` | mute (helper) | Lo lascia parlare di nuovo |
| `/warn NOME motivo` | warn (helper) | Un avvertimento; tre in 30 giorni sono un giorno di ban |
| `/note NOME testo` | notes (helper) | Una nota sul giocatore, mostrata in `/whois` |
| `/whois NOME` | whois (moderator) | Chiave, nomi usati, indirizzi, avvertimenti, note, ban |
| `/alts NOME` | alts (moderator) | Altri giocatori entrati da un indirizzo usato da questo, e quali sono bannati |
| `/players [7d]` | players (helper) | Chi è entrato di recente, visite e tempo giocato |
| `/bag NOME` | bag (moderator) | Cosa porta il giocatore, casella per casella, e i suoi zaini; offline, la borsa tenuta qui |
| `/reports` | reports (helper) | Le ultime segnalazioni |
| `/sc TESTO` | staffchat (helper) | Chat tra lo staff online |

### Chi può entrare

| Comando | Permesso | Cosa fa |
|---|---|---|
| `/whitelist on\|off\|list\|add NOME\|remove NOME` | access (admin) | Se attiva, entrano solo i giocatori in elenco e lo staff |
| `/rules set TESTO\|off` | rules (admin) | Nuove regole, che tutti devono di nuovo accettare con `/accept`; `off` le toglie |
| `/perm [PERMESSO RUOLO]` | perms (owner) | Quale ruolo chiede ogni permesso; con due parole, ne cambia uno |

### Il server

| Comando | Permesso | Cosa fa |
|---|---|---|
| `/restart [5m\|now\|cancel] [motivo]` | restart (admin) | Riavvia con avvisi su ogni schermo (10m, 5m, 1m, 30s, 10s, gli ultimi secondi); i client rientrano da soli |
| `/stop [5m\|now\|cancel] [motivo]` | restart (admin) | Lo stesso, e il server resta fermo |
| `/maintenance on [testo]\|off` | maintenance (admin) | Entra solo lo staff; gli altri vengono messi fuori con il testo |
| `/motd [TESTO\|off]` | motd (admin, per cambiarlo) | Il messaggio del giorno, detto a ogni giocatore che entra; senza parola, lo mostra |
| `/announce TESTO` | announce (moderator) | Un avviso su ogni schermo |
| `/backup` | backup (admin) | Copia subito ogni file dei mondi |
| `/metrics` | logs (admin) | Tempo acceso, giocatori e finestre per stanza, la pausa più lunga tra due tick |
| `/logs [N] [AZIONE]` | logs (admin) | Le ultime azioni di amministrazione, tutte o di un tipo (`ban`, `rollback`...) |
| `/admin` | pages (helper) | La pagina di amministrazione: chi è online, con i pulsanti per kick, ban e mute, e le pagine dietro |

### Blocchi

| Comando | Permesso | Cosa fa |
|---|---|---|
| `/inspect X Y Z [STANZA]` | inspect (helper) | Gli ultimi dieci giocatori che hanno cambiato quel blocco, da cosa a cosa, quando |
| `/rollback NOME DURATA [STANZA]` | rollback (moderator) | Rimette ogni blocco che quel giocatore ha cambiato in quel tempo |

### Portali e server amici

| Comando | Permesso | Cosa fa |
|---|---|---|
| `/portals` | portals (admin) | Dove portano le aperture di ogni stanza, e quali server mandano giocatori |
| `/friend add NOME HOST:PORTA CHIAVE [condivisi]` | federation (owner) | Un server amico; `condivisi` è un elenco tra `bans,presence,chat,bags,portals,mods`, tutti di default; un NOME già presente viene sostituito (un amico la cui chiave è cambiata) |
| `/friend remove NOME` | federation (owner) | Non più amici; i suoi ban qui vengono tolti |
| `/friend list` | federation (owner) | Gli amici, e quando si è sentito ciascuno l'ultima volta |

## Per chi fa i mod

Un mod aggiunge un comando con `core.command(nome, aiuto, permesso)` e risponde in
`core:command`; il nome è in inglese, l'aiuto una riga breve dopo `--`. Vedi
[`API.md`](API.md).
