# Roadmap

solid-space cresce per **tappe**, e ogni tappa nasce dalle **richieste di una
tappa di Tecnigrafo** ([ADR 0007](adr/0007-tappe-su-richiesta.md)): quando una
tappa di Tecnigrafo ha bisogno di qualcosa, Tecnigrafo lo chiede e qui si decide
come soddisfarlo. A ogni tappa di Tecnigrafo che chiede qualcosa corrisponde una
sola tappa di solid-space, che raccoglie tutte le sue richieste («solid-space 0.2
per Tecnigrafo 0.7»). Non ci sono tappe pianificate in anticipo. Ogni tappa ha un
obiettivo, confini precisi e criteri di chiusura verificabili; qui è aperta una
tappa alla volta, insieme alla tappa di Tecnigrafo a cui corrisponde.

Le tappe le definisce e le pubblica il responsabile del progetto. Chi contribuisce
lavora **solo** sui pacchetti della tappa aperta.

## Stato

| Tappa | Versione | Per la tappa di Tecnigrafo | Componente | Obiettivo | Nata da | Stato |
|---|---|---|---|---|---|---|
| 1 | 0.1 | — | tutti | Fondamenta: documenti, ADR, scheletro CMake, CI, sistema di verifica | fondamenta ([ADR 0001](adr/0001-scopo-e-confini.md)) | **chiusa** ([scheda](tappe/0.1-fondamenta.md), [note di rilascio](rilasci/0.1.md), [milestone v0.1](https://github.com/Matleyx78/solid-space/milestone/1), issue #4–#7 chiuse) |
| 2 | 0.2 | 0.7, comandi di modifica | `robust`, `geom2d` ([ADR 0008](adr/0008-componente-geom2d.md)) | Le decisioni e le costruzioni geometriche del piano che chiede la tappa 0.7 di Tecnigrafo | richieste #20, #21, #22 e #23 | **in preparazione** ([scheda e piano](tappe/0.2-geometria-del-piano.md)) |

Lo stato delle singole richieste (in valutazione, in lavoro, soddisfatte) non si
scrive qui: si legge nel Project «Tecnigrafo» del responsabile, che raccoglie
issue e pull request dei due repository, e nelle issue con l'etichetta
`richiesta`. I numeri di versione si assegnano in ordine di nascita (0.2, 0.3, …)
e non indicano un componente.

## Regole delle tappe

### Come nasce una tappa

1. **Richieste.** Quando prepara il piano di una tappa, o mentre la percorre,
   Tecnigrafo incontra un bisogno; il responsabile, o un agente di Tecnigrafo a
   cui lui lo chiede, apre qui una issue `richiesta` con il modulo
   ([modello](richieste/0000-modello.md)). La richiesta dice di che cosa c'è
   bisogno, non come soddisfarlo, e vive solo qui.
2. **Valutazione con contratto.** Per ogni richiesta si scrive l'ADR di
   valutazione ([modelli](adr/0000-modello-valutazione.md)), che sceglie un solo
   esito: OCCT basta così com'è, OCCT va corretto o migliorato, serve un pezzo
   nostro, oppure il bisogno non spetta a solid-space. Con gli esiti 2 e 3 la
   stessa pull request porta le intestazioni pubbliche in bozza: accettate,
   diventano il contratto su cui Tecnigrafo scrive le sue schede (ADR 0007,
   punto 15).
3. **Scheda e piano.** Le richieste con esito 1, 2 o 3 della stessa tappa di
   Tecnigrafo entrano in una sola tappa: la sua scheda rimanda agli ADR, il piano
   elenca i pacchetti.
4. **Apertura.** Quando le valutazioni sono accettate e la tappa precedente di
   solid-space è chiusa. Da quel momento la tappa avanza insieme a quella di
   Tecnigrafo.

### Stati

- **In preparazione**: esistono le richieste e se ne scrivono le valutazioni con
  il contratto, la scheda e il piano. Non si scrive codice, a parte le
  intestazioni in bozza del contratto.
- **Aperta**: la scheda è pubblicata con l'elenco dei pacchetti. È l'unica tappa di
  solid-space su cui si lavora; la tappa di Tecnigrafo a cui corrisponde è aperta
  nello stesso tempo.
- **Chiusa**: tutti i criteri sono verificati. Il documento della tappa diventa
  storia e non si modifica.

### Aprire una tappa

Solo il responsabile, e solo dopo aver chiuso la precedente: completa la scheda
(`docs/tappe/X.Y-nome.md`) e il piano a partire dagli ADR di valutazione, scrive e
accetta gli ADR che servono, elenca i pacchetti e aggiorna la tabella. Una
richiesta che nasce mentre la tappa di Tecnigrafo è in corso entra nella tappa
aperta, dopo la sua valutazione, con una riga in "Modifiche al piano".

### Durante la tappa

Ogni pacchetto unito è utilizzabile: `main` è sempre verde, e Tecnigrafo può
fissare la sua copia di solid-space al commit che chiude il pacchetto che gli
serve, senza aspettare la fine della tappa. La pull request del pacchetto elenca
in «Sblocca:» le issue di Tecnigrafo che sblocca (ADR 0007, punto 16).

### Chiudere una tappa

Tutti i pacchetti sono uniti, tutti i criteri verificati uno per uno (il «quando è
soddisfatta» di ogni richiesta è uno di essi), i test passano sui tre sistemi
operativi senza avvisi. Si aggiorna la tabella e si crea il tag `vX.Y.0`; il
responsabile controlla dal lato di Tecnigrafo i criteri delle richieste ancora
aperte e ne chiude le issue. Se ci sono richieste della tappa successiva di
Tecnigrafo, si prepara la tappa successiva.

### Cosa si può fare fuori dalla tappa aperta

- correggere errori delle tappe chiuse;
- migliorare documentazione e test;
- scrivere l'ADR di valutazione di una richiesta in attesa, con il suo contratto
  (non la sua scheda né i suoi pacchetti);
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
  ([ADR 0007](adr/0007-tappe-su-richiesta.md), accettato).
- **2026-10-27: tappa 0.1 chiusa e citazioni di tappe corrette.** Il responsabile
  dichiara chiusa la tappa 0.1 e, poiché le tappe del piano iniziale non esistono
  più, autorizza a correggere negli ADR e negli altri documenti le citazioni di
  tappe future per numero (ADR 0003, 0004, 0005 e 0006, ciascuno con la riga
  **Corretto:**).
- **2026-10-29: un flusso unico con Tecnigrafo.** Il responsabile corregge
  l'ADR 0007, che nessun codice attua ancora: una tappa di solid-space per ogni
  tappa di Tecnigrafo che chiede qualcosa, aperta insieme a quella; la
  valutazione porta il contratto (intestazioni pubbliche in bozza); la richiesta
  nasce e vive solo qui, con il modulo `richiesta`; ogni pacchetto unito è
  utilizzabile da Tecnigrafo; la verità geometrica sta in `tests/corpus/`. Le
  richieste #20–#23 della tappa 0.7 di Tecnigrafo formano la tappa 0.2. Lo stato
  delle richieste si legge nel Project «Tecnigrafo».
- **2026-10-29: architettura del piano.** Le decisioni e le costruzioni
  geometriche del piano stanno in un componente nuovo, `geom2d`, sopra
  `robust`, che tiene predicati esatti, calcolo deterministico e politica delle
  tolleranze ([ADR 0008](adr/0008-componente-geom2d.md), accettato). Le
  richieste #20, #21, #22 e #23 hanno esito 3, serve un pezzo nostro; le
  valutazioni di #23 e #20 sono gli [ADR 0009](adr/0009-regola-delle-tolleranze-del-piano.md) e
  [0010](adr/0010-posizione-e-intersezioni-nel-piano.md), accettati con il loro
  contratto: le intestazioni pubbliche di `robust` e `geom2d` che dichiarano.
  Le valutazioni di #21 e #22 sono gli
  [ADR 0011](adr/0011-raccordi-e-tangenze-nel-piano.md) e
  [0012](adr/0012-curva-parallela-nel-piano.md), accettati con il loro
  contratto: le intestazioni `tangency.hpp` e `offset.hpp` di `geom2d`.
