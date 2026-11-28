# Comandi

*[Read in English](COMMANDS.md) · [Leia em português](COMMANDS.pt.md)*

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
| `/town [town\|city]` | chiunque | Ti porta nella piazza del villaggio più vicino; con una parola qualsiasi dopo, della cittadina più vicina (un villaggio più grande, cercata al doppio della distanza); con `city`, della città più vicina (`mods/villages`) |
| `/creative TESTO` | creative (moderator) | Apre il catalogo creativo ristretto agli oggetti il cui nome contiene il testo (`mods/creative`) |
| `/zone [save\|set\|delete\|propose\|approve\|refuse\|try\|show] ...` | chiunque; `approve`, `refuse`, `try` e `show` admin (`try` e `show` chiunque nella propria officina) | Pezzi di livello (`mods/zones`). Da solo elenca le tue zone e quelle approvate. `save MONDO NOME X1 Y1 Z1 X2 Y2 Z2 [start\|room\|end]` salva quel volume come zona MONDO.NOME, tua: i blocchi varco nei lati diventano i suoi varchi, i blocchi marcatore i suoi marcatori. `set ID d MIN MAX \| weight N \| mirror on\|off \| role ...` dice a cosa serve, `delete ID` la toglie. `approve ID` di un admin la mette nei livelli di tutti e `refuse ID` (o `private`) la ritira; `propose ID` la chiede agli admin (`/page zones`); `try MONDO [QUANTE [SEME [D]]]` posa accanto a te un livello fatto con le zone di quel mondo, `show ID` quella zona da sola |
| `/workshop [leave\|blocks]` | chiunque abbia una chiave | Nella tua officina (`mods/workshop`), una stanza piatta dove si costruiscono le zone con una bacchetta, un pennello e un pannello, si provano e si propongono; `leave` esce allo spawn della prima stanza; `blocks` apre la tavolozza del pennello |
| `/myroom [whitelist\|blacklist\|add NOME\|remove NOME\|look yes\|no]` | chiunque abbia una chiave | Chi può entrare nella tua stanza personale (`mods/home`): solo i nomi della lista (whitelist) o tutti tranne loro (blacklist, quella di partenza), e se gli altri possono guardare dentro dalla tua cornice. Da solo dice la regola |
| `/myroom leave` | chiunque, nella stanza personale di qualcuno | Fuori dalla stanza, allo spawn della prima stanza del server: la via d'uscita quando la sua cornice non c'è più |

### In un solo mondo

| Comando | Mondo | Permesso | Cosa fa |
|---|---|---|---|
| `/garage` | pista | chiunque | Apre il garage: i colori e i pezzi del tuo kart; un clic lo cambia subito, anche in pista, e resta |
| `/ghost [record\|mine\|NOME\|off]` | pista | chiunque | Quale kart fantasma guida i tuoi giri: il record della pista (di base; quello dell'ingegnere sulla pista dell'Inferno), il tuo giro migliore, il giro migliore di un altro giocatore, o nessuno. Resta |
| `/laps` | pista | chiunque | Il giro migliore di tutti sull'ultima pista che hai guidato: i primi dieci e il tuo posto, e chi ha un fantasma |
| `/pieces` | pista | chiunque | Ti mette nella borsa una pila di ogni pezzo di pista, l'editor della pista (B con lui in mano sceglie da lontano il pezzo dopo), e gli strumenti linea di partenza, checkpoint, fuoripista, scatola degli oggetti e portale |
| `/manta` | mondo delle nuvole | chiunque | Quante volte hai sconfitto la Manta delle Nuvole |
| `/vehicle <jeep\|tank\|heli\|plane\|bomber\|nest\|mortar\|howitzer\|aa>` | fronte, assalto | moderator | Mette quel mezzo sei nodi davanti a te, per le prove |
| `/arpg <town\|butcher\|grave\|emberwing\|crypt\|floor1\|floor2\|keeper\|level N\|gold [N]>` | arpg | admin | Scorciatoie di prova: alla città, al boss di una zona, alla cripta o a un suo piano; il tuo livello fino a N (scelta prima una classe); N monete (1000) |
| `/backrooms <pink\|hotel\|party\|pool\|slide\|portal\|exit\|suburb\|shift\|kit>` | backrooms | admin | Scorciatoie di prova: alla porta rosa, al quartiere hotel o festa più vicino, al piano della piscina, a uno scivolo, a un luogo di portali, all'uscita, a una strada del quartiere infinito (mondo alto 96); `shift` sposta subito i collegamenti dei portali; `kit` dà le due bussole, la torcia e i materiali |
| `/chamber <0..18>` | camere di prova | admin | A quella camera, con la pistola che serve (blu dalla 03, entrambi dalla 04) |
| `/defense [kit\|turrets\|wave N]` | tower defense | chiunque (record), admin (il resto) | Senza parola, i record: l'ondata migliore del campo e di chi era, e la migliore di ogni giocatore qui. `kit` i materiali da costruzione, `turrets` due di ogni torretta e le barricate, `wave N` svuota il campo e fa partire l'ondata N |
| `/inferno <hall\|brazier 1..4\|light>` | inferno | admin | Alla sala del cuore o a un braciere; `light` li accende tutti e quattro e apre il cancello |
| `/labphase <shield\|siege\|climb\|pit\|fire\|dark\|frost\|race>` | laboratorio | admin | Lo scontro con l'Ospite da quella fase, la sala preparata (il percorso della salita, la lava, le lampade spente, il kart per la corsa) e tu al tuo posto; `pit` è la salita dal parkour della fossa |
| `/parkour <1..18\|tower 1..7\|summit>` | parkour | admin | A quella piattaforma (del livello in cui sei, in sopravvivenza), a una stazione della torre o alla cima, contata come raggiunta |
| `/plat <room\|1-1\|1-2\|1-3\|bonus\|boss\|big\|fire>` | gioco a piattaforme | admin | Fuori nella sala, dentro un livello, al tubo bonus della 1-1 o alla sala del boss della 1-3; `big` e `fire` danno quel potere in un livello |
| `/shmup <wave N\|power\|drops>` | sparatutto | admin | `wave N` fa partire subito quell'ondata con 30 vite (o al prossimo decollo); `power` riempie i potenziamenti del caccia; `drops` fa lasciare un potenziamento a ogni nemico abbattuto, o no |
| `/skies <manta\|village\|updraft\|phase2\|phase3\|kit\|reset>` | mondo delle nuvole | admin | All'isola della Manta, al villaggio o alla corrente più vicini; la fase bassa della Manta o la sua caduta subito; `kit` gli alianti, i cristalli e le armi; `reset` lo scontro da capo |
| `/track <start\|ring\|grand\|inferno\|loops\|portal\|power KIND\|kart>` | pista | admin | Alla linea di partenza di quella pista (nella sua stanza); `power KIND` (boost, boost3, banana, shell, homing, ink, star, bolt) nel tuo kart; `kart` un kart nella borsa |

## Per lo staff

### Mondo e tempo

| Comando | Permesso | Cosa fa |
|---|---|---|
| `/kill NOME` | moderator | Uccide un altro giocatore, che torna al suo letto o allo spawn (`mods/creative`) |
| `/creative on\|off` | admin | La modalità creativa per te: volo senza jetpack, nessun danno, scavo dieci volte più veloce; tenuta per nome tra un riavvio e l'altro. Libera per il proprietario di un mondo aperto dal menù del titolo (`mods/creative`) |
| `/give NOME OGGETTO [QUANTI]` | give (admin) | Mette oggetti, per nome (`chest:box`), nella borsa di un giocatore online |
| `/weather [clear\|rain\|storm] [MINUTI]` | admin | Il cielo del sandbox: senza parola, com'è e quanto dura; con una, quel cielo subito, per MINUTI o il suo tempo solito (`mods/weather`) |
| `/time [day\|noon\|dusk\|night\|midnight\|dawn\|HH:MM]` | time (admin) | L'orologio del mondo in cui sei, in ogni mondo: senza parola, l'ora; con una, quell'ora subito per tutti lì. Nel sandbox, portato indietro oltre la mezzanotte conta un giorno, come una notte dormita |
| `/story NOME [keys all\|MONDO...\|ending\|clear]` | admin | I progressi della storia di un giocatore che è nel tuo mondo: senza parola, quante chiavi e se l'Ospite è battuto. `keys all` o `keys pista inferno ...` registra quelle chiavi e le mette in borsa (mondi: pista inferno arena arpg chambers backrooms parkour); `ending` sono tutte e sette più l'Ospite battuto, che apre il fronte e l'assalto e dà i premi; `clear` toglie tutto (`mods/keys`) |
| `/season [spring\|summer\|autumn\|winter]` | admin | La stagione del sandbox (cinque giorni l'una): senza parola, quale e che giorno; con una, il primo giorno di quella stagione (`mods/weather`) |
| `/difficulty [peaceful\|easy\|normal\|hard]` | admin | Quanto è difficile il mondo, subito e dopo i riavvii (WORLD.difficulty): pacifico, nessuna creatura che combatte esce o resta -- lo spawner di un dungeon lascia quello che lascerebbero le sue creature, l'altare del profondo dà cuore e frammenti senza lotta, i villaggi non hanno assedi; facile dimezza il colpo di una creatura, quante ne escono e la fame; difficile aggiunge metà a entrambi. Senza parola, com'è (`mods/rules`) |
| `/peaceful [on\|off]` | admin | `/difficulty peaceful`, o di nuovo normale (`mods/rules`) |
| `/rule [NOME on\|off]` | admin | Le regole del mondo, dopo i riavvii (WORLD.rules): `keep_bag` (in un mondo survival la borsa resta alla morte, niente tomba), `day_cycle` (il sole si muove), `weather_cycle` (il meteo cambia da solo), `hunger` (fame). Senza parola, l'elenco (`mods/rules`) |
| `/goals` | chiunque | Quanti obiettivi del sandbox hai raggiunto; `/page goals` li elenca con un suggerimento per i prossimi (`mods/goals`) |
| `/trophies` | chiunque | Quanti trofei hai vinto; `/page trophies` li elenca, con gli indizi e la direzione dei portali nascosti (`mods/trophies`) |
| `/trophies sites` `/trophies go ID [place] [NOME]` `/trophies home [NOME]` | admin | Dove sono i cinque portali nascosti e se sono costruiti; tu o NOME davanti a uno (`well`, `summit`, `lake`, `hill`, `circle`) o, con `place`, nel posto dove porta; NOME (o tu) nella sua stanza personale, per vedere la bacheca dei trofei (`mods/trophies`) |
| `/village siege\|feast\|caravan\|incoming` | admin | Fa partire subito un evento nel villaggio in cui sei: un assedio annunciato ora e in arrivo al tramonto, la festa del raccolto, una carovana verso il villaggio più vicino, o una in arrivo da un villaggio mai visitato lungo i sentieri (`mods/villagers`) |
| `/go [ROOM]` | go (admin) | Senza parola, le stanze di questo server e se ognuna è accesa o parte a richiesta; con una, tu allo spawn di quella stanza (una a richiesta viene avviata). Poi `/tp` per un punto lì dentro |
| `/tp X Y Z` `/tp X Z` `/tp NAME` | tp (admin) | Tu in quel punto della stanza in cui sei; con solo X Z, sul terreno lì (il pavimento più alto con due nodi liberi sopra, o la tua altezza su una colonna non caricata); con un nome, da quel giocatore, o allo spawn della stanza in cui è |
| `/cursedghost` | admin | Un fantasma maledetto quattro nodi davanti a te, per provarne la presa (`mods/ghost`) |

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
| `/rollback NOME DURATA [STANZA]` | rollback (moderator) | Rimette ogni blocco che quel giocatore ha cambiato in quel tempo (serve `--block-history`) |
| `/oldbags NOME [N]` | oldbags (admin) | Le copie di una borsa rimaste indietro tra server amici; con N, restituisce quella |

### Portali e server amici

| Comando | Permesso | Cosa fa |
|---|---|---|
| `/portals` | portals (admin) | Dove portano le aperture di ogni stanza, e quali server mandano giocatori |
| `/room [list]` | rooms (owner) | Le stanze del server: accese o spente, quanti in ognuna, quali partono quando servono |
| `/room add NOME [FLAG [VALORE]...]` | rooms (owner) | Una stanza nuova nel file delle stanze (`--rooms-file`), con i suoi flag; parte quando qualcosa ci porta |
| `/room start NOME` `/room stop NOME` | rooms (owner) | Avvia subito una stanza del file; salva e ferma una stanza vuota |
| `/room remove NOME` | rooms (owner) | Toglie dal file una stanza ferma; il suo world file resta |
| `/friend add NOME HOST:PORTA CHIAVE [condivisi]` | federation (owner) | Un server amico; `condivisi` è un elenco tra `bans,presence,chat,bags,portals,mods`, tutti di default; un NOME già presente viene sostituito (un amico la cui chiave è cambiata) |
| `/friend remove NOME` | federation (owner) | Non più amici; i suoi ban qui vengono tolti |
| `/friend list` | federation (owner) | Gli amici, e quando si è sentito ciascuno l'ultima volta |

## Per chi fa i mod

Un mod aggiunge un comando con `core.command(nome, aiuto, permesso)` e risponde in
`core:command`; il nome è in inglese, l'aiuto una riga breve dopo `--`. Vedi
[`API.md`](API.md).
