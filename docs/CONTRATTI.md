# Il contratto e la sua revisione

Il **contratto** sono le intestazioni pubbliche in bozza che l'ADR di
valutazione di una richiesta porta con sé: tipi, funzioni, diagnosi ed errori
garantiti, con i loro commenti ([ADR 0007](adr/0007-tappe-su-richiesta.md),
punto 15). Accettato l'ADR, Tecnigrafo scrive le sue schede su quelle
intestazioni e i pacchetti di solid-space le realizzano senza cambiarle. I
commenti contano quanto le firme: una frase come «outOfDomain quando il raggio
supera kMaxCoordinate» è una promessa su cui Tecnigrafo costruisce i messaggi
dei suoi comandi.

Questo documento dice come si controlla che un contratto sia **completo prima
che i pacchetti partano**, e che cosa fa chi trova un buco dopo.

## Perché

Nella tappa 0.2 cinque contratti si sono rivelati incompleti a lavori iniziati:
gli zeri di `atan2` (#31); la curva parallela, per tre volte (pull request #97 e
#99, issue #103), quando i pacchetti di riferimenti si sono fermati; i cerchi
tangenti (issue #66), scoperti dal pacchetto di lavoro a metà realizzazione,
mentre P16 lavorava già sulla stessa intestazione. I buchi erano sempre degli
stessi tipi: il dominio dei risultati costruiti, i casi degeneri o infiniti, i
pareggi, come una funzione usa l'altra. Non si vedono leggendo l'intestazione:
si trovano solo cercandoli, con una lista. Da qui la revisione.

## La revisione del contratto

Ogni tappa che realizza intestazioni del contratto comincia con un pacchetto di
**revisione del contratto**, `[X.Y-C]`, prima di ogni pacchetto di riferimenti e
di lavoro che le realizza o le usa: quelle issue lo citano in «Dipende da».

- **Chi.** Un agente di fascia forte, con impegno alto, **diverso da chi ha
  scritto l'ADR** e le intestazioni: di norma Codex, perché il piano e gli ADR li
  scrive Claude con il responsabile. Difficoltà impegnativa.
- **Che cosa.** Per ogni funzione del contratto, ogni punto della lista qui
  sotto: è determinato dall'intestazione, dall'ADR o dalla scheda del pacchetto
  che la realizza (e allora si cita dove), oppure è un **buco**.
- **I buchi.** Per ciascuno una issue con l'etichetta `contratto` e la
  `tappa-X.Y`, intitolata `[X.Y-C] <intestazione>: <funzione>: <punto>` (per
  esempio `[0.2-C] tangency.hpp: tangentCircles: centri oltre il dominio`). Il
  testo dice che cosa manca, un esempio con i dati, le scelte possibili e quella
  che l'agente consiglia, con il motivo. Il nome dell'intestazione nel titolo
  serve agli agenti per riconoscere le issue che bloccano il loro pacchetto
  ([AGENTS.md](../AGENTS.md), condizione 5).
- **Il verbale.** Nella issue `[X.Y-C]`, un commento con la lista spuntata per
  ogni funzione e i numeri delle issue aperte. Se non ci sono buchi, la pull
  request del punto seguente non serve: lo dice il verbale e il responsabile
  chiude la issue.
- **Le decisioni.** Il responsabile decide ogni buco nella sua issue. Poi lo
  stesso agente, in una nuova sessione, apre **una** pull request che le
  applica: i commenti (e le dichiarazioni, se è stato deciso) delle
  intestazioni, una riga **Corretto:** nell'ADR, le schede dei pacchetti
  interessati, una voce in [DECISIONI.md](DECISIONI.md) e una riga in «Modifiche
  al piano». La pull request chiude la issue `[X.Y-C]` e quelle dei buchi.
- **Dopo il merge** partono riferimenti e lavoro. La scheda del pacchetto `C`
  autorizza le modifiche del contratto che le decisioni chiedono (ADR 0007,
  punto 15).

## La lista di controllo

Per ogni funzione, e per ogni tipo che restituisce:

1. **Dati e diagnosi.** Per ogni ingresso: che cosa è non valido, fuori dominio
   o degenere, con quale codice e quale `input`; l'ordine dei controlli quando
   più ingressi sono sbagliati; i valori al limite (zero, −0, `kMaxCoordinate`
   esatto e un ulp oltre).
2. **Dominio dei risultati.** Ogni punto, raggio o stazione costruiti può
   uscire da `kMaxCoordinate`, o dai `double`? Che cosa succede: si omette e si
   conta, si dà una diagnosi (quale, su quale `input`), si restituisce
   comunque. «Non si toglie niente in silenzio» deve avere una risposta scritta.
3. **Casi degeneri e limite.** Zero (raggio, distanza, lunghezza, ampiezza),
   curve coincidenti o sullo stesso sostegno, punti coincidenti, le uguaglianze
   esatte che separano due risposte (tangenza, distanza uguale a 2r) e un ulp
   prima e dopo ciascuna.
4. **Infinito e vuoto.** Quando le risposte sono infinite, quando non ce n'è
   nessuna, e come il risultato distingue i due casi da una diagnosi.
5. **Ordine e pareggi.** In che ordine stanno gli elementi di una lista, che
   cosa decide a pari merito, e se l'ordine è esatto o calcolato su valori
   arrotondati.
6. **Simmetria e bit.** Scambiando gli argomenti, che cosa resta uguale (gli
   stessi bit?) e che cosa si scambia.
7. **Esatto e arrotondato.** Che cosa è deciso esattamente, che cosa è
   arrotondato e con quale errore dichiarato, quali valori si restituiscono
   senza arrotondarli.
8. **Composizione.** Le funzioni che ne usano altre: quali diagnosi ed esiti
   ereditano, e se il commento di ciascuna lo dice.
9. **Coerenza.** Con le intestazioni già accettate (gli stessi nomi con lo
   stesso significato: `side()`, `Location`, le diagnosi) e con l'ADR, compresa
   la tabella dei casi della richiesta: ogni caso concreto ha una risposta
   determinata dal contratto.

Se la lista manca di un tipo di buco trovato più tardi, lo si aggiunge qui con
la decisione che lo chiude.

## I buchi trovati più tardi

La revisione riduce i buchi, non li elimina. Chi ne trova uno dopo, in un
pacchetto di riferimenti o di lavoro, **si ferma prima di scrivere codice o
casi** e apre la issue come sopra ([AGENTS.md](../AGENTS.md)): non sceglie da
solo, nemmeno segnalando la scelta, perché il contratto lo decide il
responsabile e Tecnigrafo ci ha già scritto sopra.

Finché la issue è aperta, nessun pacchetto che realizza o usa quell'intestazione
parte (AGENTS.md, condizione 5). Quando la decisione cambia un'intestazione,
chi apre la pull request che la applica lo scrive in un commento nelle issue dei
pacchetti già in lavorazione su quell'intestazione (con `AA-Presa in carico`), e
ogni pacchetto, prima di aprire la sua pull request, controlla che le
intestazioni che usa non siano cambiate in `main` da quando ha cominciato.
