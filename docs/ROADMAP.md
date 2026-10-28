# Roadmap

solid-space cresce per **tappe**, una alla volta, e ogni tappa nasce da una
**richiesta di Tecnigrafo** ([ADR 0007](adr/0007-tappe-su-richiesta.md)): quando il
lavoro a una tappa di Tecnigrafo fa emergere un bisogno, Tecnigrafo lo chiede e qui
si decide come soddisfarlo. Non ci sono tappe pianificate in anticipo. Ogni tappa
ha un obiettivo, confini precisi e criteri di chiusura verificabili; la successiva
si apre solo quando la precedente è chiusa.

Le tappe le definisce e le pubblica il responsabile del progetto. Chi contribuisce
lavora **solo** sui pacchetti della tappa aperta.

## Stato

| Tappa | Versione | Componente | Obiettivo | Nata da | Stato |
|---|---|---|---|---|---|
| 1 | 0.1 | tutti | Fondamenta: documenti, ADR, scheletro CMake, CI, sistema di verifica | fondamenta ([ADR 0001](adr/0001-scopo-e-confini.md)) | **chiusa** ([scheda](tappe/0.1-fondamenta.md), [note di rilascio](rilasci/0.1.md), [milestone v0.1](https://github.com/Matleyx78/solid-space/milestone/1), issue #4–#7 chiuse) |

Non ci sono altre tappe: la prossima nasce dalla prima richiesta di Tecnigrafo. Le
richieste in attesa sono le issue aperte con l'etichetta `richiesta`, nell'ordine
che decide il responsabile. I numeri di versione si assegnano in ordine di nascita
(0.2, 0.3, …) e non indicano un componente.

## Regole delle tappe

### Come nasce una tappa

1. **Richiesta.** Tecnigrafo, lavorando a una sua tappa, incontra un bisogno; il
   responsabile apre in solid-space una issue `richiesta`
   ([modello](richieste/0000-modello.md)). La richiesta dice di che cosa c'è
   bisogno, non come soddisfarlo.
2. **Valutazione.** Si scrive l'ADR di valutazione
   ([modelli](adr/0000-modello-valutazione.md)), che sceglie un solo esito: OCCT
   basta così com'è, OCCT va corretto o migliorato, serve un pezzo nostro, oppure
   il bisogno non spetta a solid-space.
3. **Scheda e piano.** Con gli esiti 1, 2 e 3 si scrivono la scheda della tappa,
   che rimanda all'ADR, e il piano dei pacchetti.
4. **Apertura.** Solo quando la tappa precedente è chiusa.

### Stati

- **In preparazione**: esiste una richiesta e se ne scrivono la valutazione, la
  scheda e il piano. Non si scrive codice.
- **Aperta**: la scheda è pubblicata con l'elenco dei pacchetti. È l'unica tappa su
  cui si lavora.
- **Chiusa**: tutti i criteri sono verificati. Il documento della tappa diventa
  storia e non si modifica.

### Aprire una tappa

Solo il responsabile, e solo dopo aver chiuso la precedente: completa la scheda
(`docs/tappe/X.Y-nome.md`) e il piano a partire dall'ADR di valutazione, scrive e
accetta gli ADR che servono, elenca i pacchetti e aggiorna la tabella.

### Chiudere una tappa

Tutti i pacchetti sono uniti, tutti i criteri verificati uno per uno (il «quando è
soddisfatta» della richiesta è uno di essi), i test passano sui tre sistemi
operativi senza avvisi. Si aggiorna la tabella, il responsabile controlla il
criterio dal lato di Tecnigrafo e chiude la issue della richiesta, e se c'è una
richiesta in attesa si prepara la tappa successiva.

### Cosa si può fare fuori dalla tappa aperta

- correggere errori delle tappe chiuse;
- migliorare documentazione e test;
- scrivere l'ADR di valutazione di una richiesta in attesa (non la sua scheda né i
  suoi pacchetti);
- proporre idee (una issue *Idea*): si discutono, non si implementano.

## Decisioni

Decisioni del responsabile che cambiano le regole o le tappe, in ordine di data.

- **2026-10-27: nuovo scopo.** solid-space diventa il kernel di Tecnigrafo e
  cresce a tappe piccole, nell'ordine in cui serve a Tecnigrafo
  ([ADR 0001](adr/0001-scopo-e-confini.md)). Le ambizioni del primo manifesto
  (topologia cellulare, coedge propria, raccordi propri, tolleranze rigide,
  grafo e matrici sparse, disuguaglianze, parità con Parasolid) passano
  nell'appendice "Non ora" di [VISIONE.md](VISIONE.md), con il dato che le
  riporterebbe nel piano.
- **2026-10-27: OCCT dietro una facciata.** Il nucleo è OCCT, visibile solo al
  componente `kernel`. Il responsabile chiude la discussione #104 di Tecnigrafo
  (confronto tra OCCT e un nucleo proprio) con questa scelta
  ([ADR 0002](adr/0002-componenti-e-dipendenze.md)).
- **2026-10-27: l'albero delle feature sta in solid-space** (componente
  `features`, senza interfaccia grafica), così si prova da solo. Comandi,
  interfaccia, annulla e ripeti e salvataggio nel `.cadx` restano a Tecnigrafo.
- **2026-10-27: il nome resta solid-space.**
- **2026-10-27: i due progetti hanno lo stesso responsabile.** Le interfacce tra
  i due si decidono insieme, in un ADR per parte quando serve.
- **2026-10-27: le tappe nascono da richieste di Tecnigrafo.** Le tappe 0.2–1.0
  del piano iniziale non sono più impegni (il loro testo resta nella storia di
  git). Ogni tappa parte da una richiesta e da un ADR di valutazione che decide se
  OCCT basta così com'è, se va corretto o migliorato, se serve un pezzo nostro o se
  il bisogno non spetta a solid-space
  ([ADR 0007](adr/0007-tappe-su-richiesta.md), proposto).
- **2026-10-27: tappa 0.1 chiusa e citazioni di tappe corrette.** Il responsabile
  dichiara chiusa la tappa 0.1 e, poiché le tappe del piano iniziale non esistono
  più, autorizza a correggere negli ADR e negli altri documenti le citazioni di
  tappe future per numero (ADR 0003, 0004, 0005 e 0006, ciascuno con la riga
  **Corretto:**).
