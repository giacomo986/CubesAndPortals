# M4 — Portali completi: cosa è stato costruito e cosa si è imparato

Milestone M4 di `DESIGN.md` §8: *porting della logica di `yaportal` (geometria,
trasformata, trigger, macchina a stati per giocatore, passthrough del pavimento,
roll, teletrasporto atomico), RT pool con ricorsione, ordinamento trasparenti per
vcam, cam hint, cielo/nebbia attraverso il portale.*

**In corso.** Questo documento cresce con la milestone. Sotto c'è solo ciò che è
stato costruito e verificato da esecuzioni.

---

## 1. Attraversare, che non è guardare attraverso

M0 aveva risolto la metà del *renderer*: dove mettere una camera virtuale e come
tagliare quello che vede (`core/portal/portal_math.h`). Manca l'altra metà, che
è quella che decide se un portale si può **giocare**: dove esce il giocatore,
girato dove, inclinato quanto, a che velocità.

Le due metà **non concordano, e apposta**. Il renderer usa sempre la forma
rotazione; il corpo applica l'eccezione orizzontale di §1.3 del fork. È la
ragione per cui `portal_traverse.cpp` è un file separato da `portal_math.cpp` e
non una funzione in più dentro.

### La trasformata

Con `B_s` e `B_d` le due terne come matrici di colonne `[r u n]`:

```
T = B_d · diag(-1, +1, -1) · B_sᵀ
```

Componente destra e componente normale si ribaltano, la componente su viene
portata attraverso. Entrambe le terne sono **sinistrorse** (`r × u = -n`), quindi
`det T = (-1)(+1)/(-1) = +1`: una rotazione propria. Non è un dettaglio — un
portale che specchiasse il mondo rovescerebbe la scrittura e scambierebbe le mani
al giocatore.

**L'eccezione orizzontale**: quando *uno dei due* estremi è orizzontale, il
vettore destro della sorgente viene negato prima, e la mappa effettiva diventa
`diag(+1, +1, -1)` — si ribalta solo la normale. Vale per **tutto** ciò che il
corpo porta: posizione, sguardo, velocità, su-del-mondo per il roll. Il fork
avverte che derivarla per quantità produce pose in cui posizione e sguardo non
concordano; qui è calcolata una volta e riusata.

Il suo determinante è `-1`. Per una traversata orizzontale la trasformata del
**corpo** è una riflessione, ed è voluto: i portali orizzontali sono "girami
sottosopra", non "specchiami".

### Il difetto trovato portandola: un portale a soffitto non era un portale a pavimento

La tabella delle basi del fork (`PORTAL_SYSTEM.md` §1.2) è:

```
axis 2:  n = (0,ns,0)   r = (1,0,0)    u = (0,0,1)
```

`r` è fisso mentre `n` si ribalta con `ns`. Le altre due assi ribaltano `r`
insieme a `n`, e restano sinistrorse per entrambi i versi; questa **no**: un
portale a soffitto (`ns = -1`) esce **destrorso**.

La conseguenza non è cosmetica. Il determinante della mappa del corpo è
`det(B_d)/det(B_s)`, quindi con quella tabella:

| traversata | determinante | cioè |
|---|---|---|
| pavimento → muro | −1 | riflessione |
| soffitto → muro | **+1** | **rotazione** |

Due portali orizzontali che differiscono solo per il verso trattano la stessa
traversata in modo diverso, e il documento del fork afferma la regola opposta
(«per le traversate che coinvolgono un portale orizzontale la trasformata del
corpo è una riflessione») senza che la sua stessa tabella la regga.

Qui `r = (ns, 0, 0)`. Tutte le sei combinazioni asse × verso sono sinistrorse, e
la regola diventa una frase sola: **verticale↔verticale è una rotazione,
qualsiasi cosa con un estremo orizzontale è una riflessione.**

Il check che l'ha trovato prova tutte e 24 le combinazioni asse × verso ×
rotazione. È esattamente il motivo per cui questa logica è stata portata in
`core/`: nel fork è Lua dentro un gioco in esecuzione, e l'unico modo di sapere
se una regola tiene è entrare in un portale e guardare.

### Le altre regole, ognuna con il suo gate

| Regola | Perché esiste |
|---|---|
| Clamp di profondità ±0.02 | un punto che atterra esattamente sul piano è dentro **e** fuori insieme, il trigger riparte e il giocatore rimbalza |
| Posizioni relative ai centri, direzioni no | altrimenti spostare i due portali sposta anche le direzioni |
| Uscita calcolata per l'**occhio** | ciò che dev'essere continuo è quello che si **vede**, non dove stanno i piedi |
| Occhio spinto a `0.5 + trigger` oltre la faccia | i due lati della traversata devono distare uguale, o il passaggio è uno scalino |
| Velocità come **delta** | sia l'override del server sia la fisica del client lavorano in delta; un valore assoluto verrebbe sovrascritto dal frame stesso |
| Roll misurato, non stimato | il su-del-mondo portato attraverso contro il su-del-mondo com'è, entrambi nel piano dello sguardo |
| Step-up con `warp_offset` | un portale che si apre all'altezza del ginocchio butterebbe la vista un metro in aria in un frame |
| Destinazione murata → **rifiuto** | meglio non attraversare che finire dentro la roccia |
| `triggered` per giocatore per portale | senza, si teletrasporta ogni frame passato dentro |
| Arrivo seminato `triggered = true` | senza, si rimbalza indietro appena arrivati |
| Border entry per chi cade veloce | il frame in cui era *sopra* il portale non è mai esistito, quindi senza questa clausola i portali a pavimento non scattano mai |
| Tolleranza verticale 2.0 sugli orizzontali | l'occhio sta 1.6 sopra i piedi: con 1.0 il giocatore usciva dai bounds a metà caduta e il portale gli spariva sotto |
| Bounds larghi sui frame, stretti sui block | un portale incassato in un muro non si può avvicinare di lato; uno libero sì, e scatterebbe a chi passa accanto |

### Sabotaggi

| Sabotaggio | Esito |
|---|---|
| Niente eccezione orizzontale | **rosso**, 4 check |
| Base del fork rimessa per l'asse orizzontale | **rosso**, 4 check |
| Niente clamp di profondità | **rosso**, 2 check |
| Uscita non spinta oltre la faccia | **rosso** |
| Niente fallback per il gimbal lock | **rosso** *(dopo aver riscritto il check — vedi sotto)* |
| Niente guardia `triggered` | **rosso** |
| Niente border entry | **rosso**, 2 check |
| Mappa resa uno specchio | **rosso**, 4 check |

### Un check vacuo, e cosa c'era da chiedere davvero

Il primo controllo sul gimbal lock chiedeva se lo yaw fosse **finito**. Era
*peggio* di nessun controllo, perché veniva contato: `atan2(0, 0)` vale zero, non
NaN, quindi togliendo il fallback restava verde.

Quello che il fallback compra davvero è che il giocatore continua a **essere
girato** come era girato — le spalle restano dove sono mentre la testa punta in
giù. Quindi è quello che il check chiede adesso: il vettore destro implicato
dallo yaw riportato dev'essere la trasformata del vettore destro di prima. Con il
fallback yaw = 0 e si guarda verso `(1,0,0)`; senza, yaw = π e si guarda
dall'altra parte.

Nello stesso giro: la funzione che formattava i vettori per la stampa usava **un**
buffer statico, quindi due vettori diversi nella stessa `printf` si leggevano
identici — un disaccordo si sarebbe *nascosto* invece di mostrarsi. Ora sono
quattro buffer a rotazione.

---

## 2. Attraversarlo davvero, nel gioco

La matematica di §1 non teletrasporta nessuno finché qualcuno non la chiama. Il
client adesso lo fa: due `Portal` accanto ai due `PortalLink`, uno stato di
trigger per portale, e un passo per frame subito dopo la fisica del giocatore.

Le due rappresentazioni della stessa apertura — quella che serve alla camera
(`PortalSurface`) e quella che serve al corpo (`Portal`) — sono **derivate dagli
stessi numeri** in `main()`, con asse e verso letti dalla normale. Due copie di
un'apertura sono esattamente il genere di coppia che diverge, e un renderer che
mostra un portale dove non ci si può passare è la divergenza più lenta da
diagnosticare.

Il roll finisce sul vettore *up* della camera, non nello stato del giocatore:
inclinarsi è una cosa che fa la **vista** dopo una traversata, mentre yaw e pitch
devono restare quelli che produce l'input, o muovere il mouse mentre il roll si
assesta combatterebbe contro l'animazione. Si assesta a mezzo giro al secondo:
un radiante in **20 frame, 0.33 s**.

### Una scoperta sulla scena dello spike, e una conclusione sbagliata

Il primo tentativo di gate faceva camminare il giocatore dentro il portale, e
falliva. Il codice aveva ragione: le due aperture di M0 stanno **un nodo sopra il
pavimento** — di proposito, è il caso degenerato di rasenza documentato in
`M0_FINDINGS.md` — quindi il loro bordo inferiore è sopra i piedi di chi cammina
e la cornice sotto l'apertura lo blocca.

Da lì avevo concluso, e scritto qui, che *«a piedi quei portali non si possono
imboccare»*. **Falso.** Un salto alza i piedi a circa 21.3, cioè sopra il bordo
inferiore, e si passa. L'ha trovato qualcuno provando, che è l'unico motivo per
cui si sa.

Adesso è un controllo invece che un'affermazione: il gate attraversa volando *e*
a piedi saltando, e la seconda atterra a `80.25 21.15 60.50`. La lezione è la
solita in forma nuova — un'osservazione vera («la cornice blocca chi cammina»)
non è la conclusione che sembra («non si può entrare»).

(L'altro errore del primo tentativo era mio e basta: `yaw = π` guarda verso
**+Z**, non −Z, perché `dir_from_angles` è
`(cos p·sin y, sin p, −cos p·cos y)`. Il giocatore camminava allegramente
dall'altra parte.)

### Tre controlli vacui, trovati dai sabotaggi

`tests/CMakeLists.txt` esegue `cnp_client --cross`. Quattro sabotaggi, e la
prima versione del gate ne prendeva **uno**:

| Sabotaggio | Prima | Perché era verde | Adesso |
|---|---|---|---|
| Verso della normale invertito | rosso | — | rosso |
| Arrivo non seminato | **verde** | con questa coppia l'uscita cade *fuori* dalla zona di trigger della destinazione, quindi il rimbalzo non poteva succedere comunque | rosso — si chiede lo stato direttamente |
| Roll che non si assesta mai | **verde** | questa coppia produce roll **zero**: due aperture verticali, *up* portato dritto attraverso. Chiedere se il roll è zero dopo la traversata è una domanda che si risponde da sola | rosso — si impone un radiante e si guarda se scende |
| Yaw non aggiornato | **verde** | niente controllava dove guardasse il giocatore dopo | rosso — la direzione dev'essere la trasformata di quella di prima |

Il secondo caso è quello istruttivo: il *seed* dell'arrivo è ridondante **per
questa coppia**, perché è il piazzamento dell'uscita a `0.5 + trigger` oltre la
faccia a impedire il ping-pong. Morde davvero quando la destinazione è un portale
in cui il giocatore sta **cadendo**, e quel caso appartiene al test headless. Il
gate qui chiede lo stato invece di dedurlo dal comportamento, e dice perché.

---

## 3. Ricorsione: due portali che si guardano, a profondità 2

`DESIGN.md` §8 chiede per M4 proprio il caso *«due portali che si guardano a
profondità 2 — che oggi il fork non sa fare»*. C'è.

Un pool di render target, uno per portale per profondità: `link.rt` resta la
profondità zero, il pool tiene le altre. Il rendering va **dal più profondo**,
per due ragioni che basterebbero da sole: le aperture che un passo disegna devono
già contenere un'immagine, e un render target non si può legare mentre un altro
è legato.

Il costo è esponenziale e va guardato: con due portali, profondità 0 sono 2
passaggi, profondità 1 sono 2+4, profondità 2 sono 2+4+8 = **14 passaggi di mondo
per frame**. Il default resta 1; `--recursion 2` lo alza.

Tutti i livelli sono a piena risoluzione, non per pigrizia: lo shader
dell'apertura campiona il suo target per **posizione sullo schermo**, quindi un
target a metà risoluzione verrebbe campionato nel punto sbagliato. Renderli più
economici significa cambiare quello shader, non l'allocazione.

### Il gate misura due cose diverse

`cnp_client --recursion-check`:

1. **La struttura**, che è esatta: 2, poi 6, poi 14 passaggi.
2. **I pixel**, che è la parte che il conteggio non può dire: dentro l'apertura
   del primo portale, **8.06%** dei pixel cambia passando da profondità 0 a 1, e
   **2.61%** da 1 a 2, in calo — il livello più profondo copre meno del
   precedente, come dev'essere.

### Tre cose imparate scrivendolo

**Il gate di continuità e la ricorsione si contraddicono.** Il riferimento di
`--verify` è il mondo *senza portali* visto dalla camera virtuale; con la
ricorsione accesa l'immagine del portale contiene legittimamente l'altro
portale, e il confronto falliva al **90.80%** dei pixel. Correttamente, contro la
domanda sbagliata. Quindi `--verify` forza profondità 0, dove la ricorsione è per
costruzione le due chiamate che ha sostituito.

**La scena di M0 non può mostrare la ricorsione.** I suoi due portali *non si
guardano*: uno è rivolto a +Z, l'altro a +X, venti blocchi più in là. Guardando
dentro il primo, la camera virtuale finisce dietro il secondo mentre ne esce, con
davanti solo mondo aperto: non c'è nessun secondo portale in quella vista in cui
ricorrere. La prima versione del gate riportava esattamente questo, e ci è voluto
un po' per crederle. Il gate quindi si costruisce la sua coppia — due aperture a
sei blocchi che si guardano — e prende in prestito i render target della scena.
Un mondo che *contenga* una coppia così, con le cornici, è nella lista qui sotto.

**Un gate che confronta un mondo mai sincronizzato confronta cielo con cielo.**
La prima versione non chiamava `world_render.sync`, quindi ogni profondità
tornava azzurro pieno — e tre immagini identiche danno "nessun cambiamento" con
la massima sicurezza. Diceva *nessuna ricorsione*; significava *nessun mondo*.

### Sabotaggi

| Sabotaggio | Esito |
|---|---|
| Nessuna apertura disegnata dentro i target | **rosso**, 3 check |
| Si disegna anche il portale da cui si sta guardando fuori | **verde** — non lo prende |

Il secondo è dichiarato invece che nascosto. La regola è quella di
`PORTAL_SYSTEM.md` §3.8 ed è giusta — dipingere l'apertura da cui si sta
uscendo coprirebbe tutta la vista con l'immagine di un altro posto — ma togliendo
quella riga il gate resta verde, perché il near plane obliquo è ancorato
esattamente sulla faccia su cui sta quel quad e lo taglia via per primo. Due
meccanismi che coprono un caso è difesa in profondità che funziona; è anche un
controllo che nessuno può rompere, quindi non viene contato come tale.

---

## 4. Il solido tagliato: quello che il progetto precedente non fa

Il fork non taglia niente. Un corpo a metà di un portale viene disegnato intero
da una parte e salta dall'altra quando attraversa: una cassa che entra in un
portale è una cassa che **sparisce e ricompare**. È la cosa più vistosa che gli
manca.

La regola è una frase: **la parte ancora davanti al piano d'ingresso si disegna
dov'è, tutto il resto si disegna come copia davanti al piano d'uscita.**

### Due decisioni che non sono dettagli

**Quale piano.** Il taglio è il piano per il *centro* dell'apertura, non per la
sua faccia esterna. La trasformata manda il piano centrale di A esattamente sul
piano centrale di B, quindi tagliando lì le due metà **combaciano**. Tagliando
sulla faccia, quel piano finisce sulla faccia *posteriore* di B, e la copia
comincerebbe a emergere da mezzo nodo dentro il muro.

**Quale trasformata.** Quella del renderer (forma rotazione), non quella del
corpo. Differiscono quando un estremo è orizzontale, e la copia deve accordarsi
con ciò che si vede *attraverso* il portale, perché le due cose stanno una
accanto all'altra sullo schermo e qualunque disaccordo si vede come uno scalino
sull'apertura. Conseguenza: per una coppia orizzontale la copia e il punto in cui
il corpo davvero arriva non coincidono. Il fork ha lo stesso disaccordo per la
stessa ragione; è nella lista sotto invece che nascosto.

**Il taglio è un clip plane, non geometria ricostruita.** Ricostruire una mesh
tagliata ogni frame costa una mesh ogni frame e dà comunque un solido cavo — si
vede dentro il guscio. Il clip plane costa un uniform e mostra lo stesso interno
cavo, che è anche quello che mostra un vero taglio da puzzle a portali. Tappare il taglio
con una faccia è lavoro a parte ed è nella lista.

**Le due metà si sovrappongono di un capello, non si separano di un capello.**
Entrambe tengono il piano stesso. Una sovrapposizione sulla giuntura sono due
superfici complanari alla stessa profondità dello stesso colore, cioè invisibile;
una fessura della stessa misura è lo sfondo che passa in mezzo a un corpo. È
anche il comportamento che una GPU dà gratis, perché tiene il frammento con clip
distance esattamente zero.

### La proprietà, decisa sui punti

`tests/portal_clip_test.cpp`: **ogni punto del solido è disegnato esattamente una
volta** — dall'originale se non è passato, dalla copia se è passato, mai da
entrambi e mai da nessuno. Una fessura è un corpo con una fetta mancante; una
sovrapposizione è z-fighting sul taglio. Nessuna delle due si trova guardando un
frame, perché se si vedono dipende da dove sta la camera.

```
68921 punti: 35301 qui, 35301 là, 1681 sulla giuntura, 0 nessuno
59 posizioni attraverso il piano, fessura peggiore 0, sovrapposizione peggiore 0
36 coppie di orientamento tagliate, 0 sbagliate
```

Quattro sabotaggi, tutti rossi: taglio sulla faccia invece che sul centro,
nessun test laterale (il portale diventa una lama infinita), piano d'uscita
capovolto, bounds della copia da due angoli invece che da otto. L'ultimo ha
richiesto di misurare su una coppia **non allineata agli assi**: con entrambi i
portali squadrati col mondo la mappa manda assi su assi, due angoli bastano per
caso, e il controllo non provava niente.

### Il gate grafico, e tre modi sbagliati di misurarlo

`cnp_client --cut-check`. Ha richiesto tre tentativi, e il motivo vale più del
risultato:

1. **Guardando il portale di fronte**: 44800 pixel di cassa tagliata, 44800
   intera. **Un piano perpendicolare alla linea di vista non cambia mai una
   silhouette** — la metà tolta stava esattamente dietro quella rimasta. Misura
   vera, domanda sbagliata.
2. **Guardando da dietro**: stessa trappola, stesso numero. Contava anche il
   confronto con i portali spenti, che cambia mezzo schermo per ragioni che con
   la cassa non c'entrano.
3. **Quello che funziona**: da dietro, a portali accesi in entrambi i fotogrammi,
   confrontando *tagliato contro non tagliato*. La silhouette non cambia ma il
   **colore sì**, su 46296 pixel di 61264: con la metà vicina tolta e il culling
   spento, quello che si vede attraverso il taglio è la faccia lontana **da
   dentro**, che ha un'altra sfumatura. Più un controllo obliquo, dove la
   silhouette invece cambia (29274 contro 30313).

E all'uscita: 44800 pixel cambiano con la cassa a metà dell'ingresso, **0** con
la cassa altrove.

Quattro sabotaggi, tutti rossi: niente copia, niente clip, copia con trasformata
identità, piani scambiati.

### Cosa si vede

Una cassa scorre avanti e indietro attraverso la prima apertura nel client
interattivo. È un prop dimostrativo e viene chiamato così — non c'è sistema di
item e non si raccoglie — ma un taglio è una cosa da guardare, e finché non c'è
un secondo giocatore non c'è altro in quel mondo che attraversi un portale mentre
lo si guarda. Gli altri giocatori in rete sono diventati corpi pieni per la
stessa ragione: quando i portali passeranno sulla rete, saranno tagliati anche
loro.

---

## 5. Cosa manca ancora a M4

Dichiarato adesso, non alla fine:

- ~~**La fine del tunnel**~~ — **fatta** (§10): l'apertura più profonda si
  disegna piatta nel colore del muro invece di restare un buco.
- ~~**`portal_continuity` è rosso**~~ — **verde a 0.00%** (§9), e rosso al 48%
  se l'immagine del portale viene slittata di sette pixel.
- ~~**Un mondo con una coppia di portali che si guardano**~~ — **fatto** (§8):
  `--facing`, otto nodi, spawn in mezzo, entrambe le aperture a filo del
  pavimento.
- ~~**Il taglio non è tappato**~~ — **deciso: non si tappa.** Il piano di taglio
  *coincide* con il piano del portale, quindi in un attraversamento corretto non
  esiste un punto di vista da cui il guscio cavo si veda: davanti al taglio c'è
  sempre l'apertura. L'unico posto in cui si nota è guardare attraverso il buco
  **da dietro**, e solo perché nello spike di M0 l'apertura è scavata da parte a
  parte in un muro spesso un nodo; con il retro solido non si presenta. Una
  faccia generata sarebbe geometria in più per una cosa che nessuno può guardare.
- **Un solo portale per corpo**: se un solido sta a cavallo di due portali
  insieme, viene tagliato solo sul primo. La copia andrebbe tagliata contro il
  secondo, e la sua copia contro il primo — una ricorsione nella dimensione
  sbagliata, non fatta a metà apposta.
- **Copia e arrivo non coincidono per le coppie orizzontali** (§4).
- ~~**Ordinamento dei trasparenti per camera virtuale**~~ — **fatto** (§12).
- ~~**Cam hint**~~ — **fatto** (§13).
- ~~**Cielo attraverso il portale**~~ — **fatto** (§13). **Nebbia: non esiste**
  nel motore, quindi non c'è niente da portare attraverso un portale.
- ~~**Passthrough del pavimento**~~ — **fatto** (§13), e ha portato a galla che
  il trigger orizzontale misurava la grandezza sbagliata.
- **Teletrasporto atomico** e il guard contro la corsa di `snapView` (§4.2).
- **Portali attraverso la rete**: oggi i portali esistono solo nel client in
  singolo. Il client di rete li tiene spenti, dichiaratamente.
- **La suite di casi di `PORTAL_SYSTEM.md`** che decide la milestone:
  orizzontale↔verticale (fatto, headless), mezzo-offset, giocatore dentro la
  cornice, portale dietro il vetro, teletrasporto intra-frame, e **due portali
  che si guardano a profondità 2**.

---

## 6. Il portale a filo del pavimento, e un gate che mentiva da sempre

M0 aveva misurato un difetto, dichiarato una causa e rimandato la correzione a
M4: con il bordo inferiore dell'apertura a filo del pavimento, *«la parte bassa
dell'apertura si riempie di colore vuoto»*, attribuito a un caso degenerato di
rasenza nella proiezione. È il motivo per cui le due aperture dello spike stanno
un nodo sopra il terreno, che a sua volta è il motivo per cui bisogna saltare
per attraversarle.

**La causa dichiarata era sbagliata.** Non c'entra la proiezione.

### Come è stata trovata

Riprodurre il sintomo ha richiesto tre scene sbagliate prima di quella giusta:

1. Coppia **libera nel vuoto**: misurava i quad del tunnel esposti, perché
   `build_opening` costruisce un tunnel di cinque quad pensato per stare dentro
   un muro.
2. Regione di misura **a cavallo dell'orizzonte**: lì il cielo è la risposta
   giusta.
3. Regione **rimpicciolita del 16%** ai bordi: tagliava via esattamente la banda
   in questione. Riportava `0.00%` con la banda blu visibile dieci righe sotto
   l'ultima misurata.

Poi quattro meccanismi del renderer sono stati esclusi uno alla volta — piani di
clip laterali, quinto piano, near plane obliquo con quattro valori di inset,
quad del tunnel — e **il numero non si muoveva di un pixel**. Un numero che non
risponde a niente non sta misurando niente.

### Il gate misurava un rettangolo specchiato

`project_opening` mappa la y del clip space *verso l'alto*: `+1` è in cima.
`gfx_read_screen` restituisce le righe *dall'alto in basso*: la riga 0 è in cima.
Nessuno dei due è sbagliato; insieme davano il rettangolo **specchiato rispetto
alla metà dello schermo**.

È passato inosservato attraverso il gate di continuità, quello della ricorsione e
quello del taglio perché ogni apertura che guardano sta vicino al centro
verticale, dove uno specchiamento cade comunque quasi tutto sul foro. Un'apertura
a filo del pavimento sta parecchio sotto il centro, e lì lo specchio la manda sul
muro sopra.

### La causa vera

Con la regione giusta, i quad del tunnel diventano decisivi: **50.00% → 1.14%**
togliendoli. I lati del buco campionano il render target alla propria posizione
sullo schermo, e proiettano **sotto** l'apertura della destinazione, dove il
target è stato tagliato e non contiene niente.

La correzione è una frase: l'interno di un buco nel muro è **muro**, non
l'immagine del portale. I quattro lati si disegnano in tinta piatta; solo il quad
di fondo campiona la texture. Due `gfx_draw` invece di uno.

> **Superata dal §7.** Questa correzione cura il sintomo: i lati erano il posto
> dove il difetto si vedeva, non il posto dove nasceva. La causa vera è mezzo
> nodo di immagine della destinazione tagliato via da due piani ancorati sulla
> faccia esterna del nodo invece che sul piano del portale. Il numero qui sotto
> resta valido, la spiegazione no.

```
apertura 150x439, quinto inferiore: 0 di 13200 pixel vuoti (0.00%)
```

Un portale a filo del pavimento adesso funziona. Il motivo per cui le aperture
dello spike stanno sollevate è caduto.

### E il gate di continuità adesso fallisce

Questa è la parte da non addolcire. `portal_continuity` — il criterio di uscita
n.1 di M0, quello che il progetto dichiara *superato* — **passava confrontando
due volte lo stesso muro**. Con lo specchiamento corretto riporta dall'11% al 29%
di pixel diversi dal riferimento.

Non so ancora se sia un difetto vero di continuità o un artefatto di bordo del
confronto. Allargare l'inset finché non diventa verde è esattamente la mossa che
quel gate esiste per impedire, quindi non è stata fatta: **il test resta rosso in
suite**, con il perché scritto accanto.

Il commento nel codice del gate dice già, da M0, che una versione precedente
passava perché il riferimento veniva calcolato con la stessa funzione sotto
esame. È lo stesso errore due volte, in due forme diverse: un riferimento che si
muove insieme all'oggetto misurato, e una regione che non guarda l'oggetto
misurato.

---

## 7. I lati del buco, e mezzo nodo di immagine buttato via

Il §6 si chiude con una frase che suonava definitiva: *«l'interno di un buco nel
muro è muro, non l'immagine del portale»*. Metà giusta, e la metà sbagliata era
quella che si vedeva.

Riscontro dell'utente, davanti all'eseguibile: **«i lati del buco devono mostrare
il lato opposto del portale come il fondo del buco»**. Ha ragione, e la ragione è
geometrica, non estetica. Il piano del portale passa per il **centro** del nodo:
è lì che `portal_clip` taglia un solido, è lì che scatta `portal_traverse`. Da
quel piano in poi non c'è più muro da disegnare, perché quella metà del nodo è
già l'altro posto. Dipingerla di marrone fa uscire un cassone da un tubo.

### Perché campionare i lati non funzionava

Il §6 aveva misurato la cosa giusta e concluso la cosa sbagliata: togliendo i
quad del tunnel il difetto passava da 50.00% a 1.14%, quindi i quad erano *la
causa*. Erano il **sintomo**. La causa sta due file più in là.

```
riga 852 del target = terreno di destinazione a x = 61.004
centro di B = 60.5,  faccia esterna di B = 61.0
```

Il render target non conteneva niente prima di quella riga perché **due**
meccanismi indipendenti lo tagliavano lì, e per la stessa ragione:

| dove | valore | cosa ancorava |
|---|---|---|
| `kNearPlaneInset` | `0.49 * kNodeSize` | near plane obliquo sulla faccia esterna |
| `portal_clip_planes` piano 5 | `dst.face()` | clip di profondità sulla faccia esterna |

Mezzo nodo di immagine della destinazione buttato via. Ed è esattamente il mezzo
nodo che serve: un raggio che esce dal bordo basso dell'apertura **rade il
pavimento del buco della destinazione**, che sta fra profondità 0 e 0.5. Non
c'era. Da cui il cielo in fondo al foro.

Il fatto che disabilitare uno dei due meccanismi non cambiasse il numero — e che
disabilitandoli entrambi il numero andasse a 0.00% *per il motivo sbagliato*,
cioè perché compariva il muro della destinazione — è quello che aveva fatto
concludere al §6 che nessuno dei due c'entrava.

### La correzione

Entrambi i piani ancorati sul **piano del portale**, cioè il centro del nodo.
Adesso tutto il motore concorda su dove sta un portale: il taglio dei solidi, il
trigger dell'attraversamento, il near plane e il quinto piano.

Il commento vecchio giustificava lo 0.49 con il rischio di z-fighting contro un
blocco costruito a filo del portale. Quel blocco sta a mezzo nodo di distanza da
qui, quindi il rischio non esisteva; e `0.0` è il valore che non ha bisogno di
essere giustificato.

### Il tentativo intermedio, e perché è stato buttato

Prima soluzione provata: tenere il quad immagine in fondo al buco e aggiungere un
**secondo anello** di quattro quad fra il piano del portale e la faccia lontana,
campionati invece che dipinti. Funziona a vedersi, e il gate del pavimento
diventa verde.

È sbagliato lo stesso, e a dirlo è stato il gate del taglio: quei quad
**scrivono depth** attraverso mezzo nodo dell'altro mondo, dove nascondono
qualunque cosa ci stia legittimamente. Misura: 1039 pixel della metà lontana di
un cassone spariti dietro l'anello.

Geometria finale, cinque quad invece di nove: i quattro lati del tunnel dalla
faccia del nodo **fino al piano del portale** in tinta piatta, e il quad immagine
**sul piano del portale**. L'interno del buco della destinazione, con i suoi
lati e il suo pavimento, sta già nel render target: il quad lo mostra in
prospettiva senza che serva costruirlo.

Il quad immagine è spostato di `kEdgeInset` verso chi guarda, non esattamente sul
piano. Motivo misurato: `portal_clip` taglia i solidi su quello stesso piano, per
cui la faccia di taglio di un corpo a metà è **complanare** con il quad, e chi
vince il depth test dipende dall'ordine di disegno. Senza lo scarto, un cassone a
cavallo di un portale mostrava la propria faccia di taglio attraverso l'apertura
dal lato opposto.

### Numeri

| gate | prima | dopo |
|---|---|---|
| portale a filo del pavimento | 50.00% vuoto | **0.00%** |
| continuità a 6.5 blocchi | 31.48% | 28.40% |
| continuità a 3.0 blocchi | 29.51% | 23.50% |
| continuità a 1.5 blocchi | 21.47% | **10.37%** |
| ricorsione, pixel a profondità 1 | 8.06% | 9.52% |

La continuità **resta rossa**. Si è dimezzata a distanza ravvicinata, il che dice
che una parte di quel numero era davvero il tunnel dipinto, ma il resto non è
spiegato e non viene addomesticato.

### Il gate del taglio chiedeva l'inverso della verità

Tre check su quattro sono caduti insieme alla correzione, e vale la pena scrivere
perché, perché è lo stesso errore in tre forme: **erano tarati su un difetto**.

Il check da dietro l'ingresso diceva *«tagliare cambia la maggior parte del
cassone»* e misurava 46296 pixel cambiati su 61264. Con i piani ancorati al
posto giusto, le due immagini — cassone tagliato con la copia vista attraverso
l'apertura, e cassone intero disegnato dov'è — vengono fuori **identiche byte per
byte**. Non è una coincidenza: è la definizione di continuità. La copia atterra
esattamente dove stava la metà rimossa.

Il check nuovo chiede quello: `differ == 0`. Sabotato spostando la
`clone_transform` di un decimo di nodo, va a 4840. È molto più difficile da
superare per caso di una disuguaglianza.

Il check obliquo, invece, **non fissa dove sta la copia**, ed è scritto nel
codice invece di essere lasciato da scoprire: lo stesso sabotaggio da un decimo
di nodo lungo la normale dell'uscita lascia tutti e tre i suoi numeri invariati,
perché da lì quello spostamento è quasi pura profondità. Mezzo nodo di lato li
muove (1069 diventa 6340). Quel check risponde a *«il taglio si vede da davanti»*;
quello da dietro risponde a *«ed è nel posto giusto»*.

### Una costante duplicata, trovata dal test

`tests/portal_math_test.cpp` conteneva `constexpr float kPlaneZ = 0.49f`, cioè
una seconda copia a mano del valore sotto esame. Adesso è `dst.center.z`, preso
dalla superficie stessa: se il piano si sposta di nuovo, il test si sposta con
lui invece di diventare rosso per il motivo sbagliato.

---

## 8. Una coppia frontale su cui ragionare

Il gate della ricorsione ha sempre costruito la propria coppia frontale dentro
il gate, dove nessuno può starci davanti a guardare: la coppia dello spike non
si affaccia — i due portali guardano in direzioni diverse, quindi nessuno dei
due compare nell'immagine dell'altro e ogni livello in più disegna la stessa
identica cosa. `--facing` mette quella coppia nel mondo, camminabile.

    A a 60.5 21.0 64.5 verso +X, B a 68.5 21.0 64.5 verso -X, 8 nodi
    spawn a 64.0, esattamente in mezzo, già rivolto ad A

Entrambe le aperture sono **a filo del pavimento**, che è la dimostrazione che
il §7 regge: fino a ieri quello era un difetto e la scappatoia era alzarle di un
nodo, che è il motivo per cui quelle dello spike vanno prese saltando.

### Cosa si conta guardando dentro

`--recursion N` produce **N+1 rettangoli con immagine più un buco**:

| rettangolo | cosa è |
|---|---|
| 1 | l'apertura vera, campiona il target di profondità 0 |
| 2..N+1 | un'apertura disegnata dentro il target precedente |
| ultimo | **non è un portale**: alla profondità massima le aperture non si disegnano, quindi si vede attraverso il buco nel muro |

Con `N=2` se ne contano quattro, e il quarto è erba — il terreno dietro il muro
di A. È la lacuna già dichiarata in `portal_render.h`, ed è la ragione per cui
va detta ad alta voce: sullo schermo somiglia moltissimo a un livello in più.

Il costo è stampato all'avvio dalla stessa forma chiusa che il gate verifica
contro le passate misurate: 2, 6, 14, 30.

### Tre cose trovate mettendola in piedi

**Il corridoio non resta vuoto da solo.** Le mod piazzano blocchi quando si
caricano — quattordici lo spike, tre pilastri i beacons — e il generatore mette
colonne di pietra per conto suo. Scegliere bene le coordinate non basta, perché
le mod scelgono le loro dopo. Il corridoio viene svuotato all'ultimo momento,
dopo il caricamento delle mod, e solo lo spazio strettamente fra i due muri.

**La vista partiva girata.** `GetMouseDelta()` al primo frame è la posizione del
puntatore, non uno spostamento: non esiste un frame precedente da cui misurare.
Il risultato è una rotazione di qualche centinaio di pixel prima che il giocatore
tocchi qualcosa. I primi due frame si scartano — due e non uno perché anche la
cattura del cursore lo sposta al centro, e quello spostamento cade sul frame
dopo.

**Un flag di diagnostica per due portali.** `oblique_applied()` era un booleano
solo, scritto da qualunque link fosse stato reso per ultimo. Con una coppia
frontale il giocatore guarda quasi sempre uno dei due dando le spalle all'altro,
e quello dietro spegne legittimamente il proprio piano obliquo: l'HUD diceva
`off` mentre il portale guardato lo aveva acceso. Adesso è per link.

### F5

Cicla fra vista normale, render target di A e render target di B a tutto
schermo, campionato esattamente come lo campiona un'apertura. Quando un portale
mostra la cosa sbagliata la prima domanda è se è sbagliato il target o la
lettura del target, e ogni ora spesa a indovinare fra le due è un'ora che questo
tasto costa zero.

---

## 9. La continuità: 0.00%, e la correzione applicata due volte

Il §6 ha lasciato `portal_continuity` rosso all'11–29%, il §7 l'ha portato al
10–28% senza spiegare il resto. Il resto era il gate che **applicava due volte la
stessa correzione**.

```cpp
// prima
const int row = h - 1 - y;   // "il rettangolo è bottom-up, la lettura è top-down"
// adesso
const int row = y;
```

Quel commento era vero fino al §6. Lì lo specchiamento è stato spostato dentro
`project_opening`, dove va — ed è rimasto anche qui. Un rettangolo corretto una
volta e poi corretto di nuovo è il rettangolo sbagliato di partenza, e il gate
misurava di nuovo l'apertura specchiata rispetto alla metà dello schermo. È lo
stesso difetto che il gate esiste per prendere, travestito da correzione.

```
continuity: PASS a 6.5 blocchi        0/ 13284 pixel diversi (0.00%)
continuity: PASS a 3.0 blocchi        0/ 61424 pixel diversi (0.00%)
continuity: PASS a 1.5 blocchi        0/242900 pixel diversi (0.00%)
```

**Zero esatto.** L'immagine dentro il portale e la vista di riferimento presa dal
punto ricavato a mano sono identiche byte per byte.

### Zero è anche il numero che dà un check vacuo

| Sabotaggio | Esito |
|---|---|
| Immagine del portale slittata di 7 px (`uv.x += 0.004`) | **rosso**: 48.27% / 32.99% / 22.84% |
| Mezzo grado di yaw sulla camera virtuale | **rosso**: la guardia ricavata a mano scatta prima del confronto |
| `kNearPlaneInset` rimesso a 0.49 (il difetto del §7) | **quasi verde**: 0.00% / 0.00% / **0.72%**, sotto soglia |

Il terzo è il limite di questo gate, scritto perché uno zero invita alla
conclusione sbagliata: mezzo nodo di immagine mancante non lo vede, se non
appena, da vicino. A sorvegliare quello è `portal_cut`, dove lo stesso difetto
valeva 61264 pixel su 61264.

### Il criterio di uscita n.1 di M0

`DESIGN.md` lo dichiarava superato dal primo giorno. Non lo era: la misura che lo
dichiarava confrontava due volte lo stesso muro. Adesso lo è, e la differenza fra
le due situazioni non è il numero — era già verde prima — ma il fatto che
adesso **diventa rosso quando si rompe qualcosa**.

Tre gate su questa strada avevano tutti lo stesso vizio in forme diverse: un
riferimento calcolato con la funzione sotto esame, una regione che non guardava
l'oggetto misurato, e una correzione applicata due volte. Nessuno dei tre era
visibile dal numero, tutti e tre dal sabotaggio.

---

## 10. La fine del tunnel

L'ultima voce aperta del §5. Al livello più profondo non c'è più nessun target
da campionare, e finora l'apertura non veniva disegnata affatto: il portale più
interno era un **buco** su quello che sta dietro il muro in cui è scavato. Nel
corridoio di `--facing` quel buco è un rettangolo di prato in fondo a una fila
di portali, e a colpo d'occhio non si distingue da un livello in più — chi conta
i livelli ne conta uno di troppo, che è esattamente quello che è successo la
prima volta che qualcuno si è messo nel corridoio a contare.

Adesso si disegna, piatta.

### Colore del muro, non del cielo

Il cielo era la prima scelta, ed è quella che il commento nell'header prometteva
da M0: è il colore che questo motore usa già dove non ha renderizzato niente,
compreso il clear di tutti questi target. Due cose contro.

È un **rettangolo luminoso in fondo a un corridoio scuro**, cioè tira l'occhio
esattamente dove l'immagine è più debole. Ed essendo il colore di clear, nessun
test può distinguerlo dal cielo davanti a cui sta: un gate per questo riempimento
non avrebbe avuto niente da contare. Il colore del muro dice «il buco finisce
qui», che è quello che è un portale senza niente dietro.

La costante sta nell'header (`kPortalWallColour`) e il gate la chiede da lì. Due
costanti per un colore solo sono esattamente la coppia che diverge, e il gate
sarebbe diventato verde contro il valore che si ricordava.

### Misura e sabotaggio

```
filled with wall: 15.13% a profondità 0, 6.37% a 1, 4.32% a 2
```

Contato come corrispondenza **esatta** di colore, il che è possibile solo perché
il riempimento è piatto: nient'altro in quella vista è un valore uniforme, il
mondo essendo texturizzato e ombreggiato.

| Sabotaggio | Esito |
|---|---|
| Apertura più profonda lasciata non disegnata | **rosso**: 15.13% → **0.00%**, due check |

E un avvertimento pagato sul posto: il sabotaggio è stato rimosso con
`git checkout` su un file che conteneva **anche** il lavoro non ancora
committato, che è sparito con lui. Il gate l'ha ripreso subito — 0.00% al
riavvio — ma il modo giusto di rimuovere un sabotaggio è la modifica inversa,
non il ripristino del file.

---

## 11. La suite di casi di M4, e uno che non si può ancora fare

`DESIGN.md` §8 non chiede «i portali funzionano»: chiede sei casi precisi.
Adesso ce ne sono cinque, ognuno con il suo sabotaggio, e il sesto è dichiarato
impossibile con quello che il motore ha oggi.

| Caso | Dove | Esito |
|---|---|---|
| orizzontale↔verticale | `portal_traverse` (headless, 24 combinazioni) | verde |
| **mezzo-offset** | `portal_cases` | verde, 1 pixel su 61424 |
| **giocatore dentro la cornice** | `portal_cases` | verde |
| portale dietro il vetro | — | **non coperto**, vedi sotto |
| **teletrasporto intra-frame** | `portal_cases` | verde, 0 pixel su 2073600 |
| due portali che si guardano a profondità 2 | `portal_recursion` | verde |

### Mezzo-offset

Le due aperture stanno a **offset frazionari diversi dentro i rispettivi nodi**:
A a `z = 30.5`, B a `z = 50.0`. Un portale piazzato da un giocatore non si
allinea alla griglia, e mezzo nodo è il caso peggiore perché in uno screenshot
sembra allineato lo stesso.

Sabotaggio: far assumere alla camera virtuale che ogni apertura stia al centro
del proprio nodo (`floor(v) + 0.5`). **Rosso** — e lo prende la guardia ricavata
a mano, prima ancora del confronto dei pixel. Difesa in profondità, ma va detto
quale delle due l'ha preso.

### Giocatore dentro la cornice

Con l'occhio esattamente nell'apertura, il near plane obliquo deve **rifiutare**:
applicare Lengyel lì manda il near plane dietro la camera e il portale diventa
nero. Sabotaggio: rimossa la guardia 1 di `portal_oblique_near`. **Rosso**,
`oblique ON`.

Il secondo check («lo schermo non è nero») **non** distingue quel sabotaggio:
24.46% di riquadro vuoto in entrambi i casi. È una rete più larga, per il caso in
cui il portale rinunci del tutto, e va scritto invece di lasciarlo credere più
forte di quello che è.

### Teletrasporto intra-frame

`DESIGN.md` lo formula come una camera che salta ≥ 4 nodi fra il pass RTT e il
pass dei quad. Misurato **senza camera di riferimento**, ed è questo che lo rende
esatto: il **primo** frame dopo il salto viene confrontato con lo stato assestato
venti frame dopo. Qualsiasi cosa in ritardo di un frame si vede come differenza;
nient'altro può.

```
jumped 5.0 nodes; 0 of 2073600 pixels differ from settled (0.00%)
```

Sabotaggio: dare al pass dei render target l'occhio del frame precedente.
**Rosso**, 232944 pixel su 2073600 (11.23%).

### Il caso che manca: un portale dietro il vetro

Non coperto, e dichiarato invece di sparire dalla lista. Serve un blocco che si
disegni in alpha, e questo motore **non ha trasparenza affatto**: `NodeDef` ha
`opaque` per la *luce* e niente per il rendering, il mesher emette un solo flusso
di vertici per chunk, `WorldRenderer` ha un pass solo. La regola d'ordinamento
che il renderer già rispetta — le aperture disegnate **dentro** il pass opaco,
mai dopo — esiste esattamente per questo caso ed è commentata così, ma finché non
c'è un vetro da mettere davanti a un portale è una regola senza niente che la
verifichi.

Insieme a questo cade anche **l'ordinamento dei trasparenti per camera virtuale**
della lista di §5: è lo stesso pezzo di motore mancante.

### Un gate che ha misurato un mondo a metà

Il caso mezzo-offset ha riportato **100.00% di pixel diversi** una volta e
**0.00%** tutte le volte successive. Non era il portale: `world_render.sync()` è
un **budget**, non una promessa. Costruisce il muro, sincronizza una volta,
fotografa — e la prima fotografia prende quel tanto di ricostruzione che è entrata
in quella chiamata, mentre la seconda, scattata dopo, ne prende di più.

`settle_world()` adesso cicla finché `last_meshed()` non torna zero. Un gate che
fallisce un run su quattro è peggio di un gate rosso: insegna a rilanciarlo.

---

## 12. Trasparenza, e i due item di M4 che sbloccava

Il §11 dichiarava un caso non copribile: *un portale dietro il vetro* richiede un
blocco che si disegni in alpha, e il motore non aveva trasparenza affatto. Stessa
mancanza sotto l'altro item della lista di §5, *ordinamento dei trasparenti per
camera virtuale*. Adesso c'è.

### Tre proprietà separate, non una

`NodeDef` aveva `solid` (ferma il movimento) e `opaque` (ferma la luce). Adesso
ha anche `opacity` 0..255, che è una decisione di **rendering**: 255 disegna nel
pass opaco ed è l'unico valore che non costa niente; qualunque altro sposta il
blocco nel pass alpha ordinato.

`core:glass` le usa tutte e tre in modo diverso — solido, non opaco, opacità 110
— e la quarta combinazione, una vetrata colorata che ferma la luce e si vede
comunque attraverso, è un nodo legale a un flag di distanza.

L'opacità viaggia nel **canale alpha dell'atlante**, non in una uniform. Una
uniform sarebbe una trasparenza sola per tutto il pass, quindi vetro e acqua non
potrebbero differire senza spezzare il pass in uno per valore.

### La regola che rende il vetro vetro

Il mesher emette due flussi. Una faccia traslucida **contro sé stessa** viene
scartata: un muro di vetro spesso tre disegnerebbe altrimenti sei strati di
superficie, ognuno fuso sopra il precedente, e verrebbe fuori quasi opaco.
Contro un blocco diverso — vetro sopra acqua — restano entrambe.

### L'ordinamento per camera virtuale viene gratis, e si misura

Il pass alpha ordina da lontano a vicino a partire dall'occhio **letto dalla
matrice di vista** che gli è stata passata, non da un parametro a parte:

```
per V = R · T(-eye),  la colonna di traslazione è -R·eye,  quindi eye = -Rᵀ·t
```

Il pass di un portale riceve già la matrice della camera virtuale, quindi ordina
dalla camera virtuale senza che nessuno glielo dica. Passare la posizione
dell'occhio separatamente permetterebbe alle due di divergere, e il caso in cui
divergono è esattamente il pass del portale.

`last_sort_eye()` lo rende un numero invece di un'affermazione sul codice:

```
target pass sorted from 76.50 22.00 60.50, hand derived 76.50 22.00 60.50
```

L'ordinamento è **per chunk**, non per faccia. Ordinare dentro un chunk vorrebbe
dire riordinare i vertici per camera, cioè ricostruire il vertex buffer ogni
frame; per chunk è giusto ogni volta che le superfici traslucide di uno stesso
chunk non si sovrappongono lungo la linea di vista. Una finestra ha un vetro
solo, quindi qui tiene; un acquario dentro una serra no, ed è lì che andrebbe
corretto. Scritto nel codice accanto al `std::sort`.

### Sabotaggi

| Sabotaggio | Esito |
|---|---|
| Pass alpha che scrive depth | **rosso**: 98.53% → **0.00%**, il vetro z-culla l'apertura disegnata dopo |
| Occhio dell'ordinamento preso dalla colonna di traslazione senza annullare la rotazione | **rosso**: 137.931 di distanza dal valore ricavato a mano |

Il primo è la prova che mancava da M0 alla regola *«le aperture si disegnano
dentro il pass opaco, mai dopo»*: commentata in `portal_render.h` da sempre,
senza niente che potesse verificarla finché non c'era un vetro da metterci
davanti.

### Un fallimento non riprodotto

Una singola esecuzione di `portal_cases` è fallita dentro `ctest` e non si è più
ripetuta in dodici tentativi fra suite intere e giri singoli. Causa non
identificata. Scritto qui perché un test che fallisce una volta su dodici senza
spiegazione è un debito, non un incidente: se ricapita, il primo sospetto è
un'altra istanza del client rimasta sul display.

---

## 13. Cielo, cam hint, e sprofondare in un pavimento solido

Le ultime tre voci nominate da `DESIGN.md` §8.

### Cielo attraverso il portale

In un motore con un cielo piatto solo, questo è un problema di **duplicazione**,
non di rendering: il pass principale e ogni render target facevano `gfx_clear`
con gli stessi tre letterali scritti in tre file. Cambiane uno e il cielo dentro
il portale smette di combaciare con quello attorno, su una giuntura che il
giocatore sta guardando dritta. Adesso è `kSkyColour`, uno.

**La nebbia non c'è.** Non esiste nebbia di distanza in questo motore, quindi non
c'è niente da portare attraverso un portale. Detto, invece di lasciarlo credere
coperto da una costante che sembra comprenderla.

### Cam hint

`PORTAL_SYSTEM` §2.5. Sovrascrive **dove sta** la camera virtuale di un link.

Il fork ne ha bisogno per un frame preciso — la vista di arrivo va renderizzata
*prima* che la camera del client si sia spostata — e questo motore quel frame non
ce l'ha: la traversata si applica prima di disegnare qualsiasi cosa, e il caso 3
del §11 misura il primo frame dopo un salto di cinque nodi come già assestato.
È qui per l'altro uso che il fork gli dà: pilotare l'immagine di un portale da un
punto che non è il giocatore.

Non consumante, e **solo la posizione**: sovrascrivere anche la direzione
punterebbe l'immagine dove l'apertura non può mostrarla, perché il quad campiona
il target per posizione sullo schermo. Misurato: spostarlo di quattro nodi cambia
94055 pixel, e cancellarlo riporta l'immagine a **0** pixel di differenza.

### Passthrough del pavimento, e il trigger che misurava la grandezza sbagliata

`floor_portal_cb_shift` di `PORTAL_SYSTEM` §2.3: un portale a pavimento ha un
problema che nessun portale verticale ha, cioè che qualcosa deve reggere la
cornice, e quel qualcosa sta esattamente dove il giocatore deve cadere. La
correzione alza il **fondo** della scatola di collisione finché non tocca più il
nodo, e il giocatore sprofonda.

Il gate è una **posizione prevista**, non una velocità:

```cpp
look_y = feet.y + min(0, velocity.y) * dt
shift  = clamp(top - look_y + 0.1, 0, 1.7)
```

Chi cade veloce e supera lo shift viene arrestato *sul* blocco con `vy ≈ 0`: un
gate sulla velocità direbbe «non sta cadendo», azzererebbe lo shift e lo
lascerebbe lì per sempre. Sabotaggio con un gate sulla velocità: **rosso**, due
check — il giocatore fermo sul blocco e quello arrestato.

**E poi il caso non passava lo stesso.** Con l'override attivo il giocatore
sprofondava da 25.5 a 22.306 e si fermava, senza attraversare. Il conto:

```
trigger:  occhio  < centro + 0.25 = 23.75   ⟹  piedi < 22.13
shift massimo 1.7 ⟹  piedi si fermano a 22.40
```

Mancano 27 centimetri, e non sono recuperabili: perché l'occhio arrivi a
profondità di trigger i piedi devono scendere di 1.87 sotto la faccia superiore,
e **la scatola di collisione è alta 1.8**. Nessun valore del cap chiude.

La correzione non è un numero, è la grandezza misurata. Attraverso un portale
verticale ciò che deve essere continuo è quello che si **vede**, quindi crossa
l'occhio. Attraverso un portale a pavimento è dove si **sta**: ci si passa con i
piedi per primi, e nel momento in cui i piedi sono oltre il piano si è passati.
Misurare l'occhio là significa cadere 1.6 nodi *dentro la roccia* prima che
scatti qualcosa, ed è il motivo per cui l'override doveva essere così estremo.

```
solid floor portal: with the override fell through, without it stood on it
```

Il check «senza l'override restano sopra» non è un'affermazione da poltrona: il
gate spegne l'override e guarda, nella stessa esecuzione.

---

## 14. M4 chiusa: cosa chiedeva, cosa c'è, cosa no

`DESIGN.md` §8 chiede due cose: un elenco di logica portata dal fork, e una suite
di casi che decide la milestone.

### L'elenco

| Voce | Dove | Gate |
|---|---|---|
| geometria, trasformata | `core/portal/portal_math` | `portal_math` |
| trigger, macchina a stati per giocatore | `core/portal/portal_traverse` | `portal_traverse`, `portal_crossing` |
| passthrough del pavimento | `portal_floor_shift` + `Player::cb_shift` | `portal_traverse`, `portal_crossing` |
| roll | `portal_traverse` | `portal_crossing` |
| teletrasporto atomico | `step_portals`, un solo sito | `portal_crossing` (§14 sotto) |
| RT pool con ricorsione | `PortalRenderer` | `portal_recursion` |
| ordinamento trasparenti per vcam | `WorldRenderer` pass alpha | `portal_cases` |
| cam hint | `PortalRenderer::set_cam_hint` | `portal_cases` |
| cielo attraverso il portale | `kSkyColour` | `portal_continuity` |
| nebbia attraverso il portale | — | **non esiste nebbia nel motore** |

### La suite di casi

Sei su sei, ognuno con il suo sabotaggio: §11 per i primi cinque, §12 per il
portale dietro il vetro.

### Teletrasporto atomico: perché non ha un test suo

Il fork manda posizione, vista e velocità in **un pacchetto**, perché mandarli
separati produce un frame in cui il giocatore è nel posto nuovo con
l'orientamento vecchio. Qui non c'è pacchetto: `step_portals` li scrive tutti in
un punto solo, fra la fisica e il render. Quindi la domanda non è se arrivano
insieme, ma se **concordano** — un sito unico che ricalcolasse lo sguardo dalla
posizione già spostata sarebbe atomico e sbagliato.

È quello che misura il primo caso di `portal_crossing`: il passo che riporta un
attraversamento è il passo che ha mosso **e** girato, ai valori che la
trasformata ricavata a mano dice. Due sabotaggi, entrambi presi lì:

| Sabotaggio | Esito |
|---|---|
| Yaw ricalcolato dalla posizione già spostata | **rosso** |
| Rotazione applicata un passo dopo lo spostamento | **rosso** |

Un sotto-test dedicato è stato scritto e poi **tolto**: uno dei suoi due check
chiedeva se `forward()` concordasse con lo yaw, cioè se una funzione fosse uguale
a sé stessa — `Player::forward()` è calcolata dallo yaw. L'altro chiedeva solo
«è cambiato qualcosa», e restava verde con la rotazione in ritardo di un passo
perché un attraversamento precedente aveva già lasciato un valore diverso. Due
check deboli in più non sono copertura in più, sono numeri in più da leggere.

La guardia della corsa di §4.2 non ha niente da guardare in questo client:
protegge un meccanismo di snap *pendente* da uno snap parziale che ne sovrascrive
uno pieno, e qui si applica tutto subito e non resta niente in sospeso.

### Cosa resta fuori, dichiarato

Nessuna di queste è nell'elenco di M4; sono limiti noti che vanno con le
milestone dopo.

- **Un solo portale per corpo.** Un solido a cavallo di due portali viene tagliato
  solo sul primo.
- **Copia e arrivo non coincidono per le coppie orizzontali** (§4).
- **Portali attraverso la rete.** Il client di rete li tiene spenti,
  dichiaratamente. È M5/M6.
- **Ordinamento dei trasparenti per faccia** dentro un chunk (§12).
- **Nebbia.** Non esiste.

---

## 15. Portali non dritti, e l'animazione che rimette in piedi la telecamera

Ogni portale che questo motore avesse mai disegnato, in ogni gate fino a qui,
aveva l'`up` lungo +Y e la normale lungo una delle quattro direzioni
orizzontali. È **una orientazione su ventiquattro**. Le altre ventitré non erano
vietate da niente: erano semplicemente non misurate, e tre difetti ci vivevano
dentro.

### 15.1 Cosa nascondeva

**Il costruttore del solaio sapeva fare solo muri.** `build_wall_with_opening`
sceglieva fra due formule per l'indice di blocco chiedendosi se il vettore
`right` puntasse lungo x — una domanda che ha senso solo se `up` è +Y. Un portale
a pavimento ci si costruiva attorno un muro di taglio. Riscritto nella base del
portale, senza nessun test sull'asse, e rinominato `build_slab_with_opening`
perché per un portale a pavimento il solaio *è* il pavimento.

**E sbagliava nodo quando `up` puntava in negativo.** Il centro dell'apertura sta
su un **bordo** di nodo lungo l'asse lungo, e `floor` di un punto sul bordo prende
sempre il nodo dal lato positivo: giusto finché `up` è +Y, sbagliato di un nodo
intero per una destinazione ruotata di un quarto di giro. Il buco finiva un nodo
oltre l'apertura e il primo nodo dell'apertura restava murato. Sullo schermo non
si legge come «buco spostato», si legge come «il portale mostra il pezzo di mondo
sbagliato»: **45.90%** di pixel diversi. La formula ora prende il **centro** della
cella, `center + r·dw + u·(dh + 0.5)`, e la condizione di scavo la chiede alle
semi-estensioni dell'apertura invece di scriverla come una coppia di offset
magici.

**Le due rappresentazioni divergevano.** Il corpo usa `Portal` (asse, verso,
quarto di giro), il renderer usa `PortalSurface` (centro, normale, up), e la
conversione lasciava il quarto di giro a **zero**. Giusto per un'apertura dritta,
sbagliato per ogni altra: stesso buco sullo schermo, posa d'uscita ruotata di un
quarto di giro rispetto a esso. Ora il quarto di giro si **legge** dall'`up`, e se
nessuno dei quattro combacia la conversione lo dice ad alta voce invece di
tenersi lo zero.

**Il gate di continuità era cieco proprio sulla grandezza che cambia.** Teneva la
camera di riferimento con world up su entrambi i lati — corretto per ogni coppia
con le due aperture dritte, perché il mezzo giro attorno all'up della
destinazione porta world up su world up — e **non confrontava mai `v.up` con
niente**. Ora l'up è un parametro derivato a mano come la posizione e la
direzione, ed è confrontato.

### 15.2 Tre coppie, misurate due volte ciascuna

Il rischio di una misura di continuità su una coppia orientata è che sia
**vacua**: un'apertura piena di cielo piatto concorda con qualunque posa di
camera, e uno 0.00% lì non sta leggendo niente. Quindi ogni caso è misurato
anche contro una posa **sbagliata in modo dichiarato**, e il secondo numero è
quello che dice che il primo stava leggendo qualcosa.

| Coppia | Contro la vista derivata | Contro la posa sbagliata |
|---|---|---|
| destinazione ruotata di un quarto di giro | 1 su 60726 (**0.00%**) | 85.72% (stessa camera tenuta dritta) |
| pavimento → muro | 0 su 3982 (**0.00%**) | 94.95% (stessa camera tenuta dritta) |
| pavimento → soffitto | 0 su 3982 (**0.00%**) | 74.56% (stessa camera girata di 8°) |

Il terzo caso non può usare «tenuta dritta» come posa sbagliata, e il motivo è
geometrico: pavimento e soffitto hanno lo stesso `up`, il mezzo giro è attorno a
quello, e l'up trasformato torna world up. Lì la posa sbagliata è la stessa
girata di otto gradi.

**La ROI era un rettangolo.** Due dei tre casi riportavano ostinatamente 752
pixel diversi, gli stessi 752 in entrambi. Un'apertura guardata di fronte si
proietta in un rettangolo e la scatola *è* l'apertura; guardata di sbieco — e in
un portale a pavimento non c'è altro modo di guardare, non ci si può stare a
livello del terreno — si proietta in un trapezio, il bordo lontano più corto,
e la scatola attorno prende una scheggia di solaio lungo i due lati. Geometria
vera, che legittimamente differisce fra le due immagini. Ora la regione è
`scatola AND dentro entrambe le aperture proiettate`, un **raffinamento** e non
una sostituzione: per una vista frontale toglie l'uno-due per cento di pixel di
bordo (13284 → 12960, 61424 → 60726, e nessuno a un nodo e mezzo) e ogni gate
scritto prima riporta la stessa percentuale di prima.

### 15.3 Il roll, e i trenta frame

Il roll era già in `portal_traverse` e già animato dal client. Non era mai stato
**misurato su un attraversamento che ne produce uno**: la coppia di `run_cross` ha
entrambe le aperture dritte e non produce roll, quindi il caso lì impostava
`s.roll = 1.0` a mano e guardava scendere. Un caso così resta verde anche se il
client smette del tutto di copiare il roll della traversata.

Ora l'inclinazione arriva come arriva in gioco: `portal_traverse` la calcola,
`step_portals` la copia sulla camera, la stessa funzione la smorza.

| Coppia | Roll ricavato a mano | Misurato |
|---|---|---|
| muro → muro coricato (rot 1) | −90° | −1.5708 rad |
| pavimento → muro (eccezione orizzontale) | +90° | +1.5708 rad |

I due segni opposti sono il motivo per cui vale averle entrambe: la coppia
verticale ruota, quella orizzontale riflette (determinante −1), e girano il
giocatore dalla parte opposta.

**Il frame dell'attraversamento non smorza più.** Con lo smorzamento dentro la
stessa chiamata, un quarto di giro veniva consegnato alla camera come **87.0°** e
si assestava in **29** frame: tre gradi che nessuno noterebbe, e una misura che
non corrisponde più a niente di ricavato sulla carta. L'inclinazione che la
trasformata calcola è quella che il giocatore deve *vedere* per un frame prima
che cominci a tornare. Ora: −90.0° consegnati, **30 frame esatti** (0.500 s) per
tornare a livello, mezzo giro al secondo come il fork.

### 15.4 Il mouse, mentre l'orizzonte è storto

Mancava del tutto. Il roll vive sulla camera e non in yaw e pitch — è quello che
impedisce all'animazione di litigare col mouse — ma il prezzo è che yaw gira
attorno a world up e pitch attorno all'asse destro piatto, e a vista inclinata
nessuno dei due è più un asse dello schermo. Non compensato, mezzo secondo di
assestamento dopo ogni attraversamento incrociato è mezzo secondo di comandi
diagonali, e a un quarto di giro sono **scambiati**.

Derivata nelle convenzioni di questo motore invece che trascritta dal fork (che
tiene lo yaw con novanta gradi aggiunti e il pitch col segno opposto): volendo
che la vista giri attorno agli assi *della camera inclinata*,

```
dx' = dx·cos r − dy·sin r
dy' = dx·sin r + dy·cos r
```

una rotazione piana del vettore mouse per il roll. A r = 0 è l'identità, quindi
il percorso non inclinato è intoccato per costruzione.

Il gate misura l'invariante — «spingere il mouse a destra muove la vista lungo
l'asse destro *della camera inclinata*» — a cinque roll e non a uno, perché
l'identità a zero è esattamente ciò che soddisfa anche una build non compensata.
Non compensato, la componente letta è `cos(r)`: 1.000 a 0°, 0.707 a 45°,
**0.000 a 90°**, −1.000 a 180°.

Una nota sul numero: con quaranta pixel di spinta il gate leggeva 0.9988 anche
funzionando, perché misura la **corda** fra le due direzioni di sguardo e una
corda pende dalla tangente di metà dell'angolo che sottende. Due pixel invece di
quaranta portano il residuo a 0.999997, e la soglia torna a misurare la
compensazione invece dell'arco. Abbassare la soglia sarebbe stato allargarla.

### 15.5 I sabotaggi

| Sabotaggio | Esito |
|---|---|
| compensazione del mouse tolta | **rosso** a 45°, 90°, 135°, 180° (verde a 0°) |
| quarto di giro forzato a zero nella conversione | **rosso**, 18 orientazioni su 24 |
| scavo del solaio riportato sul bordo di nodo | **rosso**, 45.90% sul caso ruotato |
| velocità di assestamento raddoppiata | **rosso**, 15 frame invece di 30 |
| `s.roll = t.roll` tolto da `step_portals` | **rosso**, 0.0° consegnati |
| il pass del portale ignora `vcam.up` | **rosso** 69.76% e 94.95%, **verde** sul terzo caso |

L'ultima riga è la più utile a leggersi: il terzo caso resta verde *correttamente*,
perché il suo up derivato è davvero world up. Un sabotaggio che tinge di rosso
tutto è un sabotaggio che non distingue.

### 15.6 Cosa resta fuori, dichiarato

**Il corpo non si inclina.** Questo motore inclina la **camera**, non il corpo.
Una destinazione coricata ha un buco largo due nodi e alto uno, e un corpo alto
1.8 non ha nessuna orientazione che ci passi: si può *guardare* attraverso un
portale coricato ed *esserne consegnati*, ma solo in spazio aperto. Non è un
dettaglio teorico — `portal_traverse` ha rifiutato l'attraversamento due volte
mentre si scriveva il gate, correttamente, e la seconda volta perché cinque
centimetri della scatola del giocatore restavano dentro il nodo del solaio, e
quei cinque centimetri sono aria solo se il corpo è allineato all'apertura.

Il gate scava la tasca d'arrivo e lo dice: senza, misurerebbe la ricerca del
gradino invece del roll.

### 15.7 Da guardare

`--oriented` mette la coppia in mondo, camminabile: un'apertura normale in cui
entrare e una coricata da cui uscire. Mezzo secondo di orizzonte che si rimette
dritto è una cosa da **guardare**, e una percentuale in un log non dice se
sembra un atterraggio o un glitch.

---

## 16. La cucitura: 0.917 nodi di mondo che saltava all'attraversamento

Trovato **giocando**, non da un gate. La rotazione funzionava, in entrambe le
direzioni; quello che non funzionava era che **la posizione del teletrasporto e
il punto di vista della telecamera non coincidevano**.

Ed è il tipo di difetto che ogni check di `portal_crossing` lasciava passare
verde: usciva alla profondità giusta, dentro i bounds, girato dove la
trasformata dice, senza rimbalzare. Tutto vero. E il mondo faceva un salto.

### 16.1 Il numero

Camminando l'attraversamento e confrontando la posa che il portale stava
**mostrando** con quella in cui il giocatore **atterra**:

```
depth_src 0.167   vcam 60.333 22.500 49.380
                  eye_after 61.250 22.500 49.380   gap 0.917
fwd  vcam 1.000 0.000 0.000   after 1.000 0.000 0.000
```

Direzione dello sguardo identica a tre decimali. **0.917 nodi** di scarto, tutti
lungo la normale d'uscita.

### 16.2 L'aritmetica

L'immagine che il giocatore guarda dentro l'apertura è disegnata da
`portal_virtual_camera`, che è la mappa e nient'altro. Quindi **l'unica**
posizione d'uscita continua è quella della mappa, e qualunque correzione
applicata sopra **è** la dimensione della cucitura.

Ce n'era una:

```c
const float adj = (0.5f + kTriggerDepth) - dot(off, n_d);
off             = off + n_d * adj;
```

Il ragionamento scritto accanto era: «il trigger scatta a quella distanza oltre
la faccia della sorgente, quindi l'uscita va messa alla stessa distanza oltre
quella della destinazione — la stessa distanza vista dai due lati». Non è né la
stessa distanza né lo stesso lato:

| | valore |
|---|---|
| `past_trigger` scatta a `depth < 0.5 − 0.25` | l'occhio è **+0.25 davanti** al piano sorgente |
| la mappa nega la componente normale | arriva a **−0.25 dietro** il piano destinazione |
| `adj` lo spingeva a `0.5 + 0.25` | **+0.75 davanti** |

Le due distanze non si annullano, **si sommano**. Con il passo reale del
giocatore la profondità al trigger era 0.167 invece di 0.25, e
0.75 − (−0.167) = **0.917**.

La spinta proteggeva da una cosa vera: la mappa lascia l'occhio **dietro** il
piano di destinazione. Ma è dove deve stare — sta attraversando. Il corpo sta
nel nodo della destinazione, che è il buco, ed esce camminando nei frame dopo.
`kDepthClamp` resta, ed è quello che impedisce di atterrare *esattamente* sul
piano, che è il caso che fa ping-pong.

### 16.3 Due gate che difendevano il difetto

Tolta la spinta, la suite è andata rossa in due punti, ed entrambi erano il
difetto scritto come requisito:

- `portal_traverse_test`: «l'occhio esce esattamente alla profondità di trigger
  oltre la faccia della destinazione» — `near(out_depth, 0.5 + kTriggerDepth)`.
- `portal_crossing`: lo stesso, sulla scena vera.

Sostituiti con la proprietà giusta, che è una sola frase in un numero solo:

| Gate | Check nuovo | Misura |
|---|---|---|
| `portal_traverse` | la profondità **si nega**, e non le succede altro | 0.2400 dentro, −0.2400 fuori |
| `portal_crossing` | l'occhio atterra dove l'apertura lo stava mostrando | **0.0000** |
| `portal_orientation` | idem, sulla coppia coricata | **0.0000** |

Non è la prima volta in questo progetto che un gate era tarato sul difetto — era
già successo al gate del taglio (§4). Il segnale è sempre lo stesso: il check
descrive un **meccanismo** invece di una **proprietà osservabile**. «Esce a 0.75
dalla faccia» è un meccanismo. «Atterra dove lo stavi guardando» è ciò che il
giocatore vede.

### 16.4 Il sabotaggio

Rimessa la spinta: **tre gate rossi su tre**, `portal_traverse`,
`portal_crossing`, `portal_orientation`, e la cucitura torna a leggere
esattamente **0.9167**.

### 16.5 Perché nessun gate l'aveva presa prima

Perché nessuno confrontava le due metà. Il caso «teletrasporto intra-frame» di
`portal_cases` misura 0 pixel su 2073600 — ma misura che il *primo frame dopo* un
salto è già il frame assestato, cioè che non c'è strappo **fra due frame dopo
l'arrivo**. Non chiede se il punto d'arrivo sia quello che il portale stava
mostrando. Sono due proprietà diverse e la seconda non aveva niente che la
guardasse.

Il commento su `kTriggerDepth` in `portal_traverse.h` affermava la simmetria che
non esisteva. Corretto anche quello: una costante documentata come «la stessa
distanza dai due lati» è esattamente ciò che scoraggia dal fare il conto.

---

## 17. La cucitura, la seconda volta: 12 cm di gradino, e una misura che si autoassolveva

§16 aveva chiuso il salto grosso e il giocatore ha detto **ancora** che il punto
d'arrivo e il punto di vista non coincidevano. Aveva ragione due volte: sul
fatto, e sulla misura che diceva di no.

### 17.1 Il check era quasi tautologico

Il check di §16 era:

```c
player_update(...);
const VirtualCamera shown = portal_virtual_camera(src, dst, player.eye(), ...);
crossed = step_portals(...);
if (crossed) seam = length(player.eye() - shown.pos);
```

`shown` è calcolata **dallo stesso occhio** che `step_portals` poi trasforma.
Quindi confronta `T(eye)` con il risultato del traverse su `eye`: dice solo che
il traverse non applica correzioni oltre la mappa. È servito — ha trovato `adj` —
ma non è una verifica della **posa d'arrivo contro l'immagine mostrata**.

E confrontava **solo la posizione**. Una traversata che atterra sul punto giusto
e gira di un grado è comunque uno strappo, e questo check restava verde.

### 17.2 La verifica vera: pixel, a schermo intero

Due render **senza portali**: uno dalla camera virtuale con cui l'apertura veniva
disegnata, uno dalla posa in cui il giocatore atterra davvero. Se
l'attraversamento è continuo sono la **stessa immagine**, su tutto lo schermo e
non dentro una regione — il frame dopo è il frame che stavi già guardando.

La prima esecuzione ha detto **76.69%**, con la posa che coincideva a 0.0000 su
posizione, direzione **e** up. Non era il codice: era la misura. `render_frame`
applica `s.roll` a qualunque up gli si passi, e la camera virtuale ha già il suo
up ruotato — il pass del portale chiama `mat4_look_to(vcam.pos, vcam.forward,
vcam.up)` e basta. Renderizzare la posa virtuale con il roll dell'attraversamento
ancora attivo **inclina due volte una base già inclinata**.

Azzerato il roll per quel render: **5 pixel su 2073600**, 0.00%.

Vale la pena dirlo chiaro perché è lo stesso errore di §9, dove il gate di
continuità applicava lo specchiamento due volte. Una correzione che vive in un
punto e viene riapplicata a valle è il difetto ricorrente di questo codice.

### 17.3 Il fatto, che c'era davvero

La coppia del gate atterra in spazio sgombro. La **scena** no:

```
--oriented --cross
came out at 60.33 20.00 59.38
the eye landed 0.1200 from where the opening was showing it
```

**0.12 nodi**, verticali. È lo **step-up**: l'occhio d'uscita cade a y 21.5, i
piedi a 19.88, dentro il terreno; `portal_traverse` li solleva a 20.00 e l'occhio
sale con loro. Un salto verticale nell'unico frame in cui il giocatore sta
guardando l'immagine con attenzione.

`Traversal::warp_offset` porta quel numero da quando la traversata è stata
scritta, con il commento che dice cosa farne — «il corpo sta a `feet`; alla
camera si dice di partire `warp_offset` più in basso, all'uscita senza cuciture,
e di salire in una frazione di secondo» — e **nessuno lo leggeva**.

### 17.4 La correzione

Il corpo sta sollevato, che è ciò di cui ha bisogno la fisica; la camera parte
all'uscita continua e recupera in `kCamWarpTime = 0.15 s`, che è ciò di cui ha
bisogno l'occhio. Due risposte a una domanda che ne ha davvero due.

Un **tempo** fisso e non una velocità: un sollevamento di due centimetri e uno di
un nodo devono finire nello stesso sguardo, mentre a velocità costante il primo
sarebbe istantaneo e il secondo un ascensore.

| | prima | dopo |
|---|---|---|
| cucitura nella scena `--oriented` | 0.1200 | **0.0000** |
| recupero della camera | mai | **9 frame** (0.150 s) |

### 17.5 I check nuovi

La posa non basta a se stessa, quindi ora sono tre numeri più i pixel:

- posizione, direzione **e** up della posa d'arrivo contro quella mostrata:
  0.0000, 0.0000, 0.0000
- l'immagine d'arrivo contro l'immagine mostrata, schermo intero: **5 pixel su
  2073600**
- un caso che ha **davvero** un gradino, con un check che lo dice: *sollevato di
  0.1200, quindi questo caso non è vacuo*. Senza, un mondo sgombro riporta zero e
  il gate si congratula.

### 17.6 I sabotaggi

| Sabotaggio | Esito |
|---|---|
| la camera ignora `warp_offset` | **rosso**, cucitura 0.1200 |
| il warp non si riassorbe | **rosso**, 600 frame invece di 9 |
| (§16) la spinta rimessa | **rosso** su tre gate, cucitura 0.9167 |

---

## 18. Un portale disegnato a 79 che teletrasporta da 60

Segnalazione: «un portale è posizionato a 79, 20, 60 ma il suo teleport è a
60, 20, 60». Misurato in `--oriented`, stampando le due rappresentazioni dopo
120 frame:

```
renderer B 79.50 22.00 60.57      body B 60.50 21.50 60.00
```

**Diciannove nodi.** L'apertura veniva disegnata in un posto e attraversata da un
altro, con entrambe le metà che si comportavano perfettamente ciascuna per conto
suo.

### 18.1 Due difetti, uno sopra l'altro

**Un'apertura esiste in TRE posti, non due.** Due nel renderer — sorgente di un
link e destinazione dell'altro — e una come `Portal`, che è il tipo che il corpo
attraversa. `host_set_portal` scriveva i primi due. Il commento sopra la funzione
diceva perfino «both have to move together, or the pair starts showing a view
from somewhere the portal no longer is», riferendosi alle due copie *dentro il
renderer*: la terza non era dimenticata, era fuori dal campo visivo di chi
scriveva.

**E il mod scriveva coordinate assolute.** `mods/spike` muove il portale B ogni
frame per dimostrare che uno script può farlo, e lo faceva verso i letterali
`79.5, 22.0, 60.5` — il portale B della scena spike, scritto a mano. In quella
scena era invisibile; in `--facing` e `--oriented` trascinava l'apertura
dall'altra parte del mondo, ogni frame.

I due insieme danno la segnalazione esatta: il mod sposta l'apertura *disegnata*
a 79.5 e il corpo resta a 60.5.

### 18.2 Le correzioni

`host_set_portal` scrive tutte e tre le copie. Solo il **centro**: l'asse, il
verso e il quarto di giro sono la forma dell'apertura, non la sua posizione, e
un'API che li cambiasse scrivendo tre float sarebbe un'API che ricostruisce il
portale di nascosto.

E c'è `core.get_portal(index, axis)`, che è il motivo per cui il mod scriveva
letterali: senza una lettura, un mod che *muove* un portale non ha modo di
muoverlo **relativamente** a dov'è. Un float e un indice d'asse invece di tre
valori di ritorno, perché il generatore emette un solo ritorno e allargarlo per
una funzione sola è uno scambio peggiore di questa forma.

| | prima | dopo |
|---|---|---|
| `--oriented`, renderer vs corpo | 79.50 22.00 60.57 vs 60.50 21.50 60.00 | **60.50 21.50 60.07** entrambi |
| scena di default | 79.50 22.00 60.57 | **79.50 22.00 60.57** entrambi, invariata |

### 18.3 Il gate, e le due volte che era cieco

Prima versione: sposta il portale con l'API, controlla le tre copie, **ripristina**,
poi fa girare i mod un secondo e chiede se il portale è rimasto. Verde col
sabotaggio dei letterali rimessi — perché il gate gira la **scena spike**, dove
quei letterali sono giusti. Una domanda su «hardcodato a una scena» va posta da
un'altra scena.

Seconda versione: non ripristina, fa girare i mod col portale già spostato. Rosso
sempre, anche col mod corretto — e giustamente: il mod legge la posizione **una
volta, al load**, quindi non seguire un portale spostato sotto di lui dopo non è
un difetto, è proprietà. Il check stava misurando una cosa diversa da quella che
diceva.

Terza: sposta il portale, **ricarica il mod**, poi misura. Entrambe le metà sono
portanti — spostare rende la domanda posta da un'altra scena, ricaricare la rende
leale.

| Sabotaggio | Esito |
|---|---|
| `host_set_portal` non scrive la copia del corpo | **rosso**, metà 0.226 apart |
| il mod rimette i letterali `79.5, 22.0, 60.5` | **rosso**, spostato di 51.746 |

---

## 19. M4 è finita? Sì, e il modo in cui lo so è cambiato

L'elenco di `DESIGN.md` §8 è coperto e la suite di casi che la milestone nomina
come prova d'uscita è verde, sei su sei. Ma §14 diceva già questo, e poi sono
uscite tre cuciture, nessuna delle quali quella suite vedeva.

| Cucitura | Valore | Trovata da |
|---|---|---|
| spinta a `0.5 + kTriggerDepth` dalla faccia | 0.9167 nodi | **giocando** (§16) |
| step-up non assorbito dalla camera | 0.1200 nodi | **giocando** (§17) |
| l'apertura vive in tre posti, l'API ne scriveva due | 19 nodi | **giocando** (§18) |

Tutte e tre erano invisibili a venti test verdi, e due erano *scritte come
requisito* in un check che le proteggeva. La lezione non è che mancava un caso:
è che i sei casi misurano il **renderer** contro un riferimento e nessuno di essi
attraversava un portale e chiedeva se il frame dopo era il frame di prima. Adesso
lo chiede `portal_orientation`, a pixel pieni.

### Cosa resta fuori, aggiornato

Oltre a quanto §14 già dichiara (un solo portale per corpo, disaccordo
copia/arrivo per le coppie orizzontali, portali in rete, ordinamento per faccia,
nebbia inesistente):

- **Il corpo non si inclina.** Solo la camera. Un'apertura coricata è larga due
  nodi e alta uno, e nessun corpo alto 1.8 ci passa: si guarda e se ne esce, non
  ci si entra (§15.6).
- **`velocity_delta` non è misurata su un attraversamento.** È sommata dopo
  `player_update`, quindi il frame della traversata potrebbe applicare un passo
  nella direzione vecchia. Nessun gate lo guarda.
- **`cb_shift` attraverso un pavimento.** È azzerato dentro il teletrasporto
  mentre `update_floor_shift` gira prima di `player_update`: c'è un frame in cui
  l'altezza della scatola cambia sotto la camera. Non misurato.

Le ultime due sono della stessa famiglia delle tre cuciture sopra — un frame in
cui due grandezze non concordano — e sono scritte qui perché è l'unico posto in
cui una cosa non fatta resta visibile.

---

## 20. I due dubbi di §19 erano due bug

§19 li aveva dichiarati «non misurati». Misurati, erano entrambi difettosi.

### 20.1 La velocità di un attraversamento moriva in un frame

Una caduta dentro un portale a pavimento che esce da un muro trasforma velocità
verso il basso in velocità in avanti. È l'unica cosa che un portale fa che nessun
altro elemento di un gioco a voxel fa, e niente la misurava.

```
fell at 0.00 -11.67 0.00      came out 11.67 0.00 0.00      a tick later -0.00 -0.42 0.00
```

`velocity_delta` era giusta e `step_portals` la sommava correttamente. Il frame
**dopo** la buttava via: `player_update` **assegna** le componenti orizzontali,

```c
player.velocity.x = wish.x * speed;      // assegna
player.velocity.z = wish.z * speed;
player.velocity.y += kGravity * dt;      // accumula
```

quindi al tick successivo la velocità portata viene sostituita da quel che dicono
i tasti, che per un giocatore a mezz'aria senza tasti premuti è **zero**. Solo la
componente verticale sopravviveva, ed è l'unica scritta con `+=`.

**Correzione.** A terra i tasti assegnano, che è quel che rende il camminare
immediato. In aria possono solo **aggiungere**, lungo la direzione richiesta, fino
alla velocità di camminata *in quella direzione*: mai sottrarre.

```c
const float along = velocity.x*dx + velocity.z*dz;
const float add   = min(kAirAccel * dt, speed - along);
```

`kAirAccel = 12` m/s²: da fermo servono 0.4 s per raggiungere il passo in una
nuova direzione — abbastanza per sterzare, non abbastanza per fermarsi. Senza
tasti la velocità è conservata esattamente.

Dopo: `a tick later 11.67 -0.42 0.00`.

**Nota per M5/M6.** `MovementLimits::max_horizontal` è `kFlySpeed`, e con la
tolleranza fa 22.4 m/s. Una caduta lunga mappata su orizzontale la supera —
terminale è 60. Oggi non morde perché il client di rete tiene i portali spenti;
morde quando li accende. Registrato come **OPEN-1** in `TODO.md`, che è dove
`DESIGN.md` M-1 chiede che stiano i bug aperti — ed è lo stesso bug che il fork
aveva come BUG-10, con la mitigazione già progettata in `DESIGN.md` 3.4 (grace
token).

### 20.2 Uscire da un portale a pavimento faceva cadere fuori dal mondo

```
wall into floor: 1 crossings, ended at 50.50 29.30 50.62
```

Il pavimento era a y 30. Il giocatore è finito **sotto**.

`portal_floor_shift` testa solo la posizione **laterale**: niente limita quanto
sotto l'apertura possano stare i piedi. E l'uscita da un portale a pavimento è
proprio là sotto — la mappa mette il corpo dietro il piano di destinazione, che
per un'apertura rivolta in su vuol dire sotto. Quindi la scatola restava
sollevata di 1.7 su 1.8 per tutto il tempo in cui il giocatore restava allineato
all'apertura, e non collideva con niente.

Il colpevole è il **clamp**:

```c
return (shift > kMaxFloorShift) ? kMaxFloorShift : shift;   // era
if (shift > kMaxFloorShift) return 0.0f;                    // è
```

Oltre il cap il giocatore non sta sprofondando *attraverso* l'apertura, ci è già
passato sotto: rifiutare, non limitare. È anche la risposta giusta per il caso che
il cap proteggeva — un faller veloce collide, viene arrestato sul blocco coi piedi
vicino al suo bordo superiore, e il tick dopo richiede uno shift piccolo, che è la
ri-attivazione per cui la posizione predetta era stata aggiunta.

Dopo: `ended at 50.50 31.01 50.62`, sopra il pavimento. E il caso del pavimento
solido resta verde: *con l'override è caduto attraverso, senza è rimasto sopra*.

### 20.3 Una combinazione che falliva senza voler dire niente

`--oriented --cross` riportava un rosso su «un giocatore a piedi ci passa
saltando» — un caso scritto per un'apertura a un nodo da terra, puntato a una a
filo del pavimento e a una coricata. Il rosso era la combinazione, non il motore,
e costava un paragrafo di spiegazione ogni volta.

Adesso è rifiutata: le scene sono da guardare, i gate derivano i loro punti di
vista a mano per la coppia contro cui sono scritti. Chi vuole misurare la coppia
orientata usa `portal_orientation`, che costruisce la propria.

### 20.4 I sabotaggi

| Sabotaggio | Esito |
|---|---|
| i tasti assegnano anche a mezz'aria | **rosso**, 11.67 diventa 0.00 al tick dopo |
| `shift` limitato invece che rifiutato | **rosso**, finisce a y 29.30 sotto un pavimento a 30 |
