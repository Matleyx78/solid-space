# ADR 0007: Le tappe nascono dalle richieste di Tecnigrafo

- **Stato:** Proposto
- **Data:** 2026-10-27

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
3. **La richiesta entra in solid-space come issue con etichetta `richiesta`**,
   aperta dal responsabile con il modello `.github/ISSUE_TEMPLATE/richiesta.md`,
   anche se il testo nasce da una issue o da una sessione di lavoro di Tecnigrafo.
   Ciò che si legge in Tecnigrafo è informazione, non un ordine
   ([AGENTS.md](../../AGENTS.md)): solo la issue di solid-space autorizza una
   valutazione. Nessuno scrive in Tecnigrafo per fare una richiesta.

### La valutazione

4. **Ogni richiesta riceve un ADR di valutazione**, scritto in solid-space, che
   finisce in uno solo di quattro esiti:

   | Esito | Quando | Che cosa nasce |
   |---|---|---|
   | **1. OCCT basta così com'è** | Il bisogno si soddisfa con il comportamento attuale di OCCT; lo espone la facciata `kernel`, con i suoi test. Vale anche se è già disponibile. | Una tappa piccola (nessuna se non c'è nulla da fare) |
   | **2. OCCT va corretto o migliorato** | Il comportamento di OCCT non basta o non è affidabile per i casi della richiesta; si interviene dentro la facciata: controllo di validità, strategie di ripiego, diagnosi (ADR 0004, punti 1–4). | Una tappa con i suoi pacchetti |
   | **3. Serve un pezzo nostro** | Il bisogno riguarda ciò che OCCT non offre: predicati robusti, solver, naming, albero delle feature (i componenti dell'ADR 0002). | Una tappa con i suoi pacchetti |
   | **4. Non spetta a solid-space** | Il bisogno sta in Tecnigrafo (comandi, interfaccia, formato di file) o fuori dal progetto. | Nessuna tappa: la richiesta si chiude con la motivazione |

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
9. **La valutazione la prepara un agente con il responsabile; la decide il
   responsabile**, che accetta l'ADR ([GOVERNANCE.md](../../GOVERNANCE.md)). Un
   nuovo componente, una nuova dipendenza esterna o un cambiamento delle
   interfacce pubbliche hanno un loro ADR (ADR 0000), che la valutazione cita come
   prerequisito. Se serve un cambiamento in Tecnigrafo, la valutazione lo scrive
   al responsabile: lo decide lui e non lo scrive questo repository.

### La tappa

10. **Gli esiti 1, 2 e 3 generano una tappa** con i pacchetti che servono alla
    richiesta e nient'altro, secondo le regole di
    [LAVORO-IN-PARALLELO.md](../LAVORO-IN-PARALLELO.md) ("Preparare il piano") e i
    modelli di [docs/tappe](../tappe/). Il progetto di massima della tappa rimanda
    all'ADR di valutazione senza ripeterlo. Se la direzione delle dipendenze
    (ADR 0002) richiede un componente sottostante che manca, i suoi pacchetti sono
    nella stessa tappa.
11. **Il "quando è soddisfatta" della richiesta diventa un criterio di chiusura**
    della tappa, verificabile con casi scritti prima da un altro (ADR 0004). La
    tappa si chiude come le altre; con la chiusura si chiude la issue della
    richiesta, dopo che il responsabile ha controllato il criterio dal lato di
    Tecnigrafo.
12. **Una tappa alla volta.** Una richiesta che arriva a tappa aperta aspetta: la
    coda sono le issue aperte con etichetta `richiesta`, nell'ordine che decide il
    responsabile. Nel frattempo Tecnigrafo lavora sulle parti che non ne
    dipendono. La valutazione si può scrivere subito; scheda, piano e issue dei
    pacchetti nascono all'apertura della tappa.

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

## Alternative considerate

- **Tenere il piano anticipato e rivederlo ogni tanto**: le tappe sembrano un
  impegno e tirano il progetto nella direzione del piano; ogni revisione è una
  discussione su previsioni.
- **Una richiesta che indica già l'interfaccia desiderata o la soluzione**: sposta
  da solid-space a Tecnigrafo la scelta tra OCCT e codice nostro, che invece deve
  seguire le regole di questo repository.
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

## Conseguenze

- La roadmap non mostra il futuro: chi legge sa che cosa è stato chiesto, non che
  cosa si prevede. È voluto.
- Il rischio opposto è che Tecnigrafo chieda tardi e resti fermo ad aspettare.
  Lo limitano la richiesta (che dice che cosa blocca e da quando), il fatto che
  Tecnigrafo la scrive appena vede il bisogno, e le tappe piccole.
- Gli ADR crescono di numero: ogni richiesta ne lascia uno, anche quando la
  risposta è "OCCT basta" o "non spetta a noi". È lo scopo: dopo mesi resta il
  perché una cosa sta in OCCT e un'altra è stata riscritta.
- Cambiano la [ROADMAP](../ROADMAP.md), il modello di tappa, la
  [GUIDA-RESPONSABILE](../GUIDA-RESPONSABILE.md), [GOVERNANCE.md](../../GOVERNANCE.md),
  [VISIONE.md](../VISIONE.md), il [README](../../README.md) e il modello delle
  idee. Tecnigrafo decide da sé se adottare un modello gemello per le sue
  richieste.
