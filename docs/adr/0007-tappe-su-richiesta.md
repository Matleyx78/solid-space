# ADR 0007: Le tappe nascono dalle richieste di Tecnigrafo

- **Stato:** Accettato
- **Data:** 2026-10-27
- **Corretto:** 2026-10-29 — il responsabile cambia la cucitura tra i due
  progetti; nessun codice unito attua ancora questo ADR (eccezione di
  [GOVERNANCE.md](../../GOVERNANCE.md)). Sei correzioni: una tappa di
  solid-space per ogni tappa di Tecnigrafo che chiede qualcosa, aperta insieme a
  quella (punti 10–12); la valutazione porta con sé il contratto, cioè le
  intestazioni pubbliche in bozza (punto 15); la parte sugli esiti 1 e 2 si
  accorcia quando un vincolo già deciso esclude OCCT (punto 8); la richiesta
  nasce e vive solo qui, scritta con il modulo `richiesta` (punto 3); ogni
  pacchetto unito è utilizzabile da Tecnigrafo (punto 16); la verità geometrica
  sta nell'archivio dei casi (punto 17). Di conseguenza cambiano anche il
  contesto, le alternative e le conseguenze.
- **Corretto:** 2026-11-01 — il responsabile aggiunge al punto 15 la revisione
  del contratto prima dei pacchetti, dopo che nella tappa 0.2 cinque contratti
  si sono rivelati incompleti a lavori iniziati (issue #31, #66 e #103, pull
  request #97 e #99): un pacchetto `[X.Y-C]` di un agente diverso da chi ha
  scritto l'ADR controlla il contratto con una lista fissa
  ([CONTRATTI.md](../CONTRATTI.md)), il responsabile decide i buchi, e solo
  allora partono riferimenti e lavoro; chi trova un buco più tardi si ferma prima
  di scrivere codice o casi. Il principio del punto non cambia.

## Contesto

L'[ADR 0001](0001-scopo-e-confini.md) stabilisce che solid-space cresce a tappe
piccole, nell'ordine in cui Tecnigrafo ha bisogno dei pezzi. La roadmap però
elencava in anticipo le tappe 0.2–1.0, con componenti e contenuti scritti prima
che Tecnigrafo incontrasse il bisogno. Un piano scritto in anticipo è una
previsione: può far costruire ciò che non serve, o nell'ordine sbagliato, e
orienta il lavoro verso il piano invece che verso Tecnigrafo.

Chiusa la tappa 0.1, il responsabile (lo stesso dei due progetti) decide che sia
Tecnigrafo a far nascere le tappe. Quando Tecnigrafo aprirà una sua tappa (per
esempio la 0.7, quella delle modifiche 2D) e incontrerà un bisogno (per esempio i
predicati geometrici robusti), lo chiederà a solid-space; solid-space decide come
soddisfarlo e da quella richiesta, non da un calendario, nasce la sua tappa.

La richiesta può essere di qualunque tipo, non solo di codice nuovo. Decidere se
basta OCCT così com'è, se va corretto o migliorato secondo le regole già accettate
([ADR 0002](0002-componenti-e-dipendenze.md) e
[ADR 0004](0004-robustezza-e-verifica.md)), se serve un pezzo nostro o se il
bisogno non spetta a solid-space è compito di solid-space.

Questo ADR attua il punto 3 dell'ADR 0001 senza cambiarlo.

**La correzione del 2026-10-29.** La prima tappa di Tecnigrafo che ha chiesto
qualcosa, la 0.7, ha portato quattro richieste insieme (#20–#23). Con le regole
di partenza (una tappa per richiesta, una tappa alla volta, il testo scritto in
Tecnigrafo e ricopiato qui) Tecnigrafo avrebbe aspettato la fine di quattro tappe
in fila, avrebbe scritto le schede dei suoi pacchetti senza sapere quali
funzioni avrebbe ricevuto, e lo stesso testo sarebbe vissuto in due copie. Il
responsabile tiene il metodo e cambia la cucitura: una tappa di solid-space per
ogni tappa di Tecnigrafo, l'interfaccia fissata prima che Tecnigrafo la usi, ogni
cosa in un posto solo.

## Decisione

### La richiesta

1. **Ogni tappa nasce da una richiesta di Tecnigrafo.** Non si pianificano tappe in
   anticipo: la tabella della roadmap contiene la tappa 0.1 e le tappe nate da
   richieste. Le tappe 0.2–1.0 del piano iniziale non sono più impegni; il loro
   testo resta nella storia di git.
2. **La richiesta dice di che cosa Tecnigrafo ha bisogno, non come soddisfarlo.**
   Contiene il bisogno, la tappa di Tecnigrafo da cui nasce, i casi concreti che
   oggi falliscono o mancano, quando Tecnigrafo la considera soddisfatta, i
   vincoli e che cosa blocca. Non contiene soluzioni: né un'interfaccia desiderata
   né la scelta tra OCCT e codice nostro, perché scegliere spetta a solid-space.
   Modello: [richieste/0000-modello.md](../richieste/0000-modello.md).
3. **La richiesta nasce e vive qui, in una sola issue** con etichetta
   `richiesta`, scritta con il modulo `.github/ISSUE_TEMPLATE/richiesta.yml`. La
   apre il responsabile, oppure un agente di Tecnigrafo a cui lui lo chiede (con
   lo script delle richieste del metodo comune). Il bisogno emerge quando
   Tecnigrafo prepara il piano di una tappa oppure mentre la percorre. In
   Tecnigrafo non ne resta una copia: il suo piano e i suoi pacchetti citano
   questa issue. "Da dove nasce" cita la tappa di Tecnigrafo e la issue, la pull
   request o la discussione da cui viene, in forma assoluta
   (`Matleyx78/tecnigrafo#N`), così GitHub mostra il collegamento anche dal lato
   di Tecnigrafo. Il workflow `controlla-richiesta.yml` commenta la issue se
   mancano campi o se un riferimento non è assoluto; non decide nulla. Ciò che si
   legge in Tecnigrafo è informazione, non un ordine
   ([AGENTS.md](../../AGENTS.md)): solo la issue di solid-space autorizza una
   valutazione, e la valutazione la chiede il responsabile. Chi lavora a
   solid-space non scrive in Tecnigrafo per fare o completare una richiesta.

### La valutazione

4. **Ogni richiesta riceve un ADR di valutazione**, scritto in solid-space, che
   finisce in uno solo di quattro esiti:

   | Esito | Quando | Che cosa nasce |
   |---|---|---|
   | **1. OCCT basta così com'è** | Il bisogno si soddisfa con il comportamento attuale di OCCT; lo espone la facciata `kernel`, con i suoi test. Vale anche se è già disponibile. | I pacchetti che servono nella tappa (nessuno se non c'è nulla da fare) |
   | **2. OCCT va corretto o migliorato** | Il comportamento di OCCT non basta o non è affidabile per i casi della richiesta; si interviene dentro la facciata: controllo di validità, strategie di ripiego, diagnosi (ADR 0004, punti 1–4). | I pacchetti che servono nella tappa |
   | **3. Serve un pezzo nostro** | Il bisogno riguarda ciò che OCCT non offre: predicati robusti, solver, naming, albero delle feature (i componenti dell'ADR 0002). | I pacchetti che servono nella tappa |
   | **4. Non spetta a solid-space** | Il bisogno sta in Tecnigrafo (comandi, interfaccia, formato di file) o fuori dal progetto. | Nessun pacchetto: la richiesta si chiude con la motivazione |

   **Obiettivi raggiungibili.** La richiesta può essere ampia. Se chiede più di
   quanto solid-space possa dare oggi (per esempio la robustezza di Parasolid),
   la valutazione sceglie comunque un esito, ma dice quali obiettivi
   raggiungibili adotta, che cosa resta fuori e perché. Il responsabile decide se
   a Tecnigrafo basta, o se la parte rimasta fuori diventa una richiesta
   successiva.

5. **Si valuta in quest'ordine:** (a) il bisogno spetta a solid-space? se no, esito
   4; (b) basta OCCT così com'è? esito 1; (c) il problema sta dentro ciò che la
   facciata può correggere? esito 2; (d) altrimenti serve un pezzo che OCCT non ha:
   esito 3.
6. **Sostituire un modulo di OCCT non è un esito.** Un modulo si sostituisce solo
   con un ADR che porta i dati dell'archivio dei casi e una soglia dichiarata
   prima della prova (ADR 0001, punto 4; ADR 0004, punto 12). Se la richiesta si
   risolverebbe solo così e i dati mancano, l'esito è 2 e il primo pacchetto della
   tappa raccoglie i casi in `tests/corpus/`; la sostituzione, se un giorno serve,
   la propone un ADR a parte. Le idee dell'appendice "Non ora" di
   [VISIONE.md](../VISIONE.md) restano lì finché un dato non le riporta nel piano.
   I sottomoduli non si modificano (ADR 0002, punto 9).
7. **La valutazione dichiara su che cosa si basa** (comportamento documentato di
   OCCT, archivio dei casi, una prova eseguita) **e che cosa non ha potuto
   verificare.** Se la prova fatta durante la tappa smentisce l'esito, un nuovo
   ADR sostituisce il primo: non si riscrive.
8. **Due formati.** Completo (richiesta, valutazione, alternative, decisione,
   conseguenze, verifica) per gli esiti 2 e 3; breve, di poche righe, per gli
   esiti 1 e 4. L'intestazione è la stessa nei due e porta l'esito e la richiesta,
   così si filtra senza aprire i file. Modelli:
   [0000-modello-valutazione.md](0000-modello-valutazione.md).
   **Quando decide un vincolo già preso**, la valutazione lo cita invece di
   rifare il confronto con OCCT. Per il 2D l'ADR 0030 di Tecnigrafo (punto 5:
   `core` compila senza librerie esterne, quindi senza OCCT) esclude gli esiti 1
   e 2 una volta per tutte: la valutazione risponde ai passi (b) e (c) del
   punto 5 con quella citazione, in una o due righe, e dedica il suo spazio a
   come costruire il pezzo nostro e al contratto (punto 15).
9. **La valutazione la prepara un agente con il responsabile; la decide il
   responsabile**, che accetta l'ADR ([GOVERNANCE.md](../../GOVERNANCE.md)). Un
   nuovo componente, una nuova dipendenza esterna o un cambiamento delle
   interfacce pubbliche hanno un loro ADR (ADR 0000), che la valutazione cita come
   prerequisito. Se serve un cambiamento in Tecnigrafo, la valutazione lo scrive
   al responsabile: lo decide lui e non lo scrive questo repository.

### La tappa

10. **Una tappa di solid-space per ogni tappa di Tecnigrafo che chiede
    qualcosa.** La tappa di solid-space raccoglie tutte le richieste di quella
    tappa di Tecnigrafo che hanno esito 1, 2 o 3, e prende il nome da entrambe
    («solid-space 0.2 per Tecnigrafo 0.7»). Contiene i pacchetti che servono a
    quelle richieste e nient'altro, secondo le regole di
    [LAVORO-IN-PARALLELO.md](../LAVORO-IN-PARALLELO.md) ("Preparare il piano") e i
    modelli di [docs/tappe](../tappe/). Ogni richiesta conserva il suo ADR di
    valutazione; il progetto di massima della tappa rimanda agli ADR senza
    ripeterli. Se la direzione delle dipendenze (ADR 0002) richiede un componente
    sottostante che manca, i suoi pacchetti sono nella stessa tappa.
11. **Il "quando è soddisfatta" di ogni richiesta diventa un criterio di
    chiusura** della tappa, verificabile con casi scritti prima da un altro
    (ADR 0004). La issue di una richiesta si chiude quando il responsabile ha
    controllato il criterio dal lato di Tecnigrafo: appena i pacchetti di quella
    richiesta sono uniti, al più tardi con la chiusura della tappa. La tappa si
    chiude come le altre.
12. **Una tappa aperta per progetto, e le due avanzano insieme.** Comanda il
    piano di Tecnigrafo. Quando Tecnigrafo prepara una tappa che chiede
    qualcosa, solid-space ne valuta le richieste e prepara la tappa
    corrispondente; la apre appena le valutazioni sono accettate e la sua tappa
    precedente è chiusa, e da quel momento le due tappe sono aperte insieme.
    Tecnigrafo non aspetta la chiusura della tappa di solid-space ma il singolo
    pacchetto che gli serve (punto 16), e nel frattempo lavora sulle parti che
    non ne dipendono. Una richiesta che nasce mentre Tecnigrafo percorre la sua
    tappa entra, dopo la valutazione, nella tappa aperta di solid-space che le
    corrisponde, con una riga in "Modifiche al piano". Le richieste di una tappa
    successiva di Tecnigrafo aspettano la chiusura della tappa aperta di
    solid-space; le loro valutazioni si possono scrivere subito, e una richiesta
    che blocca la tappa aperta di Tecnigrafo passa prima delle altre. Le
    richieste aperte sono le issue con etichetta `richiesta`; il loro stato si
    legge nel Project «Tecnigrafo» del responsabile, che raccoglie issue e pull
    request dei due repository, non in un documento. Scheda, piano e issue dei
    pacchetti nascono all'apertura.

### Numeri delle tappe

13. **I numeri di versione si assegnano in ordine di nascita** (0.2, 0.3, …) e non
    indicano più un componente.
14. **Le tappe future non si citano per numero.** Negli altri documenti si cita
    l'evento che le fa nascere («la tappa che porta OCCT», «la prima tappa con
    codice geometrico da provare»). Le citazioni per numero del piano iniziale
    (ADR 0003, 0004, 0005 e 0006, ARCHITETTURA, CODING_STYLE, AMBIENTE-SVILUPPO,
    VISIONE e `tests/corpus/README.md`) sono state corrette il 2026-10-27 con
    l'autorizzazione del responsabile; ogni ADR toccato porta la riga
    **Corretto:**. La scheda e le note di rilascio della tappa 0.1 restano com'erano:
    sono storia.

### Il contratto

15. **La valutazione porta il contratto.** Quando un esito 2 o 3 aggiunge o
    cambia un'interfaccia pubblica, la pull request della valutazione contiene
    anche le intestazioni pubbliche in bozza: le dichiarazioni dei tipi, delle
    funzioni e delle diagnosi che Tecnigrafo userà, che compilano senza
    implementazione, e per ogni costruzione l'errore garantito. Stanno dove le
    userà il codice (`<componente>/include/solid_space/…`) e seguono le regole di
    sempre: niente tipi di dipendenze esterne (ADR 0002), errori come valori con
    diagnosi (ADR 0003). Diventano il contratto tra i due progetti quando il
    responsabile accetta l'ADR: Tecnigrafo scrive le schede dei pacchetti che le
    usano su queste intestazioni, mentre i pacchetti di solid-space le realizzano.
    Da quel momento cambiarle è una decisione dei due progetti: se ne discute in
    una issue con etichetta `contratto`, decide il responsabile, e la scheda del
    pacchetto che le cambia lo autorizza. Una funzione dichiarata ma non ancora
    realizzata non si usa finché il suo pacchetto non è unito.

    Prima dei pacchetti che lo realizzano, il contratto si rivede (corretto il
    2026-11-01): il primo pacchetto della tappa, `[X.Y-C]`, appena l'ADR è
    accettato e anche con la tappa in preparazione, fatto da un agente diverso
    da chi ha scritto l'ADR, controlla ogni funzione con la lista di
    [CONTRATTI.md](../CONTRATTI.md) (diagnosi, dominio dei risultati, casi
    degeneri e infiniti, ordine e pareggi, simmetria, composizione, coerenza) e
    apre una issue `contratto` per ogni punto che il contratto non determina. Il
    responsabile decide; una sola pull request applica le decisioni; poi partono
    riferimenti e lavoro, e Tecnigrafo scrive le schede che usano il
    contratto. Un buco trovato più tardi ferma chi lo trova prima del
    codice o dei casi, e ferma i pacchetti che usano quell'intestazione finché
    non è deciso ([AGENTS.md](../../AGENTS.md), "Il contratto").

### Il pacchetto unito

16. **Ogni pacchetto unito è utilizzabile.** `main` è sempre verde, e ciò che un
    pacchetto realizza è completo e provato quando la sua pull request è unita.
    Tecnigrafo può fissare la sua copia di solid-space al commit che chiude un
    pacchetto, senza aspettare la fine della tappa. La pull request del pacchetto
    elenca nella riga «Sblocca:» le issue di Tecnigrafo che sblocca, scritte come
    `Matleyx78/tecnigrafo#N`: GitHub mostra il rimando in quelle issue senza che
    nessuno scriva in Tecnigrafo. Alla chiusura della tappa si crea il tag
    `vX.Y.0`.

### La verità geometrica

17. **La verità geometrica sta nell'archivio dei casi.** I casi concreti delle
    richieste e i file DXF di AutoCAD che arrivano da Tecnigrafo entrano in
    `tests/corpus/` come casi di verifica, scritti da un pacchetto di riferimenti
    con le regole di sempre (ADR 0004 e 0005: valori attesi indipendenti dal
    programma, provenienza, licenza). I riferimenti di Tecnigrafo citano questi
    casi invece di rifare i calcoli, e verificano la traduzione dalle entità alle
    curve e i comandi. Se un riferimento di Tecnigrafo e l'archivio non
    concordano, si apre una issue in solid-space e decide l'archivio; se è
    l'archivio a essere sbagliato, lo corregge un pacchetto di riferimenti
    ([tests/corpus/README.md](../../tests/corpus/README.md), "Disaccordi con
    Tecnigrafo").

## Alternative considerate

- **Tenere il piano anticipato e rivederlo ogni tanto**: le tappe sembrano un
  impegno e tirano il progetto nella direzione del piano; ogni revisione è una
  discussione su previsioni.
- **Una richiesta che indica già l'interfaccia desiderata o la soluzione**: sposta
  da solid-space a Tecnigrafo la scelta tra OCCT e codice nostro, che invece deve
  seguire le regole di questo repository. Il contratto del punto 15 non la
  reintroduce: le intestazioni le scrive solid-space dopo aver scelto l'esito.
- **Un ADR solo quando cambia l'architettura** (la regola di ADR 0000): chi
  valuta deve ogni volta giudicare se la soglia è superata, e una regola che
  chiede giudizio si erode. Con un ADR per richiesta la regola è semplice; il
  costo, molti ADR piccoli, si limita con il formato breve e l'intestazione.
- **La valutazione come sezione della scheda della tappa**: le richieste senza
  tappa (esito 4) non avrebbero dove scriverla, e la decisione starebbe in due
  posti.
- **Far partire la valutazione dalla issue di Tecnigrafo, senza una issue in
  solid-space**: gli agenti agirebbero su un testo che il responsabile non ha
  ammesso (AGENTS.md).
- **Una tappa per richiesta, una alla volta** (il testo del 2026-10-27): le
  richieste di una stessa tappa di Tecnigrafo diventano tappe in coda, e
  Tecnigrafo aspetta la fine di ciascuna anche quando gli serve un solo pezzo.
- **La richiesta scritta in Tecnigrafo e ricopiata qui** (il testo del
  2026-10-27): due testi da tenere allineati a mano, e un passaggio in più per il
  responsabile a ogni richiesta.
- **Il contratto nel primo pacchetto della tappa**: Tecnigrafo potrebbe scrivere
  le schede che dipendono da solid-space solo dopo quel pacchetto, cioè di nuovo
  in fila invece che in parallelo.
- **Calcolare la geometria attesa da entrambe le parti**: i calcoli delicati si
  farebbero due volte, e due riferimenti diversi per lo stesso caso non dicono
  quale sia giusto.

## Conseguenze

- La roadmap non mostra il futuro: chi legge sa che cosa è stato chiesto, non che
  cosa si prevede. È voluto.
- Il rischio opposto è che Tecnigrafo chieda tardi e resti fermo ad aspettare.
  Lo limitano la richiesta (che dice che cosa blocca e da quando), il fatto che
  Tecnigrafo la scrive appena vede il bisogno, il contratto fissato prima delle
  sue schede e i pacchetti utilizzabili appena uniti.
- Gli ADR crescono di numero: ogni richiesta ne lascia uno, anche quando la
  risposta è "OCCT basta" o "non spetta a noi". È lo scopo: dopo mesi resta il
  perché una cosa sta in OCCT e un'altra è stata riscritta.
- Le due tappe aperte insieme chiedono al responsabile di seguire due piani; il
  Project «Tecnigrafo» li mostra sulla stessa tavola. Le automazioni preparano
  commenti e pull request, ma unire, accettare e chiudere restano decisioni sue.
- Un'intestazione accettata costa di più da cambiare: è voluto, perché
  Tecnigrafo ci ha già scritto sopra le sue schede. L'etichetta `contratto` rende
  visibile ogni cambiamento.
- Cambiano la [ROADMAP](../ROADMAP.md), il modello di tappa, la
  [GUIDA-RESPONSABILE](../GUIDA-RESPONSABILE.md), [GOVERNANCE.md](../../GOVERNANCE.md),
  [VISIONE.md](../VISIONE.md), il [README](../../README.md) e il modello delle
  idee. Tecnigrafo decide da sé se adottare un modello gemello per le sue
  richieste. Con la correzione del 2026-10-29 cambiano anche il modulo della
  richiesta (`.github/ISSUE_TEMPLATE/richiesta.yml`), il modello di pull request
  (riga «Sblocca:»), il workflow `controlla-richiesta.yml`, il modello delle
  valutazioni e [tests/corpus/README.md](../../tests/corpus/README.md); dal lato
  di Tecnigrafo cambia l'ADR 0030, con una pull request di Tecnigrafo decisa dal
  responsabile.
