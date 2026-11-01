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
che i pacchetti partano**, che cosa fa chi trova un buco dopo, e come si
controlla una correzione del contratto prima di unirla.

## Perché

Nella tappa 0.2 cinque contratti si sono rivelati incompleti a lavori iniziati:
gli zeri di `atan2` (#31); la curva parallela, per tre volte (pull request #97 e
#99, issue #103), quando i pacchetti di riferimenti si sono fermati; i cerchi
tangenti (issue #66), scoperti dal pacchetto di lavoro a metà realizzazione,
mentre P16 lavorava già sulla stessa intestazione. I buchi erano sempre degli
stessi tipi: il dominio dei risultati costruiti, i casi degeneri o infiniti, i
pareggi, come una funzione usa l'altra. Non si vedono leggendo l'intestazione:
si trovano solo cercandoli, con una lista. Da qui la revisione.

Lo stesso giorno in cui è nata la revisione, una correzione ha aperto un buco
nuovo: la pull request #128 ha fissato il bulge della curva parallela **dati gli
estremi arrotondati**, ma ha lasciato liberi gli estremi entro l'errore, e
cinquanta minuti dopo il merge R19 si è fermato perché due scelte lecite degli
estremi danno esiti diversi (issue #132). Una correzione è un contratto nuovo
scritto in fretta, e nessuno l'aveva ripassato con la lista. Da qui la
revisione delle correzioni e i punti 10 e 11 della lista.

## La revisione del contratto

Ogni tappa che realizza intestazioni del contratto comincia con un pacchetto di
**revisione del contratto**, `[X.Y-C]`, prima di ogni pacchetto di riferimenti e
di lavoro che le realizza o le usa: quelle issue lo citano in «Dipende da».

- **Quando.** Appena gli ADR di valutazione della tappa sono accettati, anche se
  la tappa è ancora in preparazione: la issue `[X.Y-C]` si crea allora, con
  l'etichetta `tappa-X.Y`, senza aspettare l'apertura. Così la revisione non
  rallenta nessuno, e **Tecnigrafo scrive le schede dei pacchetti che usano il
  contratto dopo che la revisione è chiusa**, non prima: le scrive su un
  contratto già completo.

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
  Anche questa pull request cambia il contratto: prima del merge passa la
  revisione delle correzioni (sotto), fatta da un agente diverso da chi la
  scrive.
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
10. **Scelte libere entro un errore.** Dove il contratto lascia libero un
    valore entro un errore dichiarato (come si arrotonda una coordinata, quale
    `double` si sceglie per un estremo, una stazione), quella scelta può
    cambiare qualcosa di discreto: un esito, una diagnosi, un ordine,
    un'uguaglianza, un pareggio? Si prova con un esempio al limite, con due
    scelte entrambe lecite. Se sì, il contratto fissa la scelta (per esempio
    «il `double` più vicino, pareggio al pari») oppure dice che l'esito ne
    dipende, e allora nessun caso di riferimento può imporlo (issue #132).
11. **Che cosa arriva al chiamante.** Ciò che la funzione decide esattamente
    (quale elemento è quale, l'ordine, l'appartenenza a un pezzo,
    l'uguaglianza di due punti) arriva nel risultato, o chi lo usa dovrebbe
    ricostruirlo dai valori arrotondati? Due elementi distinti possono avere
    gli stessi valori arrotondati (due tagli a meno di un ulp)? Se sì, il
    risultato porta l'informazione esatta (un indice, un ordine, un flag),
    oppure il commento dice che i valori arrotondati non la conservano.
    Vale anche per le schede di Tecnigrafo che passano i risultati da una
    funzione all'altra (Matleyx78/tecnigrafo#359).

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

## Le correzioni del contratto

Ogni pull request che cambia un'intestazione del contratto già accettata,
anche solo un commento, si rivede con la lista **prima del merge**. Vale per
quella che applica la decisione di un buco, per quella del pacchetto `[X.Y-C]`
e per quella di un pacchetto la cui scheda autorizza a cambiare il contratto.

- **Chi la scrive.** In testa alla descrizione mette la riga
  **Revisione del contratto: da fare**, con le intestazioni e le funzioni che
  cambia.
- **Chi la rivede.** Un agente di fascia forte, con impegno alto, diverso da
  chi l'ha scritta: di norma Codex, perché le correzioni le scrive di solito
  Claude con il responsabile; Claude se l'ha scritta Codex. La prende in carico
  come ogni pull request (AGENTS.md, «Per prima cosa: prendi in carico») e non
  la modifica.
- **Che cosa.** Ogni punto della lista, sulle funzioni che la pull request
  cambia e su quelle che le usano o ne usano i risultati (punto 8), **sul testo
  dopo la correzione**: una correzione chiude un buco, ma spesso ne apre uno
  accanto (punti 7, 10 e 11). Un punto già deciso altrove si cita, non si
  ridiscute.
- **Il verbale.** Un commento nella pull request che comincia con
  **Verbale della revisione del contratto**, con la lista spuntata per ogni
  funzione e i numeri delle issue aperte. Per ogni buco nuovo, una issue
  `contratto` come in «La revisione del contratto».
- **Il merge.** Il responsabile unisce solo dopo il verbale. Se ci sono buchi
  nuovi li decide prima: la decisione entra nella stessa pull request, che
  allora si rivede di nuovo sulle sole funzioni toccate dalla decisione.
