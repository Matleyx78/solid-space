# Roadmap

solid-space cresce per **tappe**, nell'ordine in cui Tecnigrafo ne ha bisogno. Ogni
tappa ha un obiettivo, confini precisi e criteri di chiusura verificabili. Le tappe
si percorrono una alla volta: la successiva si apre solo quando la precedente è
chiusa.

Le tappe le definisce e le pubblica il responsabile del progetto. Chi contribuisce
lavora **solo** sui pacchetti della tappa aperta.

## Stato

| Tappa | Versione | Componente | Obiettivo | Serve a Tecnigrafo per | Stato |
|---|---|---|---|---|---|
| 1 | 0.1 | tutti | Fondamenta: documenti, ADR, scheletro CMake, CI, sistema di verifica | tutto | **in chiusura: prova manuale da completare** ([scheda](tappe/0.1-fondamenta.md), [note di rilascio](rilasci/0.1.md), [milestone v0.1](https://github.com/Matleyx78/solid-space/milestone/1), issue #4–#7 chiuse) |
| 2 | 0.2 | `robust` | Predicati `orient2d` e `incircle`, poi 3D; politica delle tolleranze | tappa 0.7 (modifiche) | in programma |
| 3 | 0.3 | `solver` | Interfaccia del solver e primo motore su libslvs, schizzi 2D | tappa dei vincoli 2D | in programma |
| 4 | 0.4 | `naming` | `StableId`, grafo di genealogia, salvataggio | fase 2 | in programma |
| 5 | 0.5 | `kernel` | Facciata OCCT: primitive, estrusione, booleane, raccordi, STEP, controllo di validità, strategie di ripiego, archivio dei casi | fase 2 | in programma |
| 6 | 0.6 | `kernel`, `naming` | Genealogia delle operazioni collegata al naming | fase 2 | in programma |
| 7 | 0.7 | `features` | Albero delle feature e rigenerazione senza interfaccia | fase 2 | in programma |
| 8 | 1.0 | tutti | Stabilità, prestazioni, benchmark pubblico | fase 2 | in programma |

Le tappe 0.2 e 0.3 hanno una scadenza che dipende da Tecnigrafo: `robust` deve
essere pronto prima che Tecnigrafo apra la sua tappa 0.7, e `solver` prima della
tappa dei vincoli 2D. Quando Tecnigrafo modifica il proprio calendario, il
responsabile aggiorna questa tabella.

### Contenuto previsto delle tappe in programma

Schede complete si scrivono all'apertura di ciascuna tappa. Qui il minimo per
orientarsi.

- **0.2, `robust`.** Dentro: `orient2d`, `incircle`, `orient3d`, `insphere` a
  precisione adattiva; politica delle tolleranze come tipo documentato. Chiusura:
  i predicati coincidono con un oracolo in aritmetica razionale esatta, anche su
  punti quasi allineati e quasi cocircolari.
- **0.3, `solver`.** Dentro: entità e vincoli 2D di base (coincidenza,
  parallelismo, perpendicolarità, tangenza, distanza, angolo, raggio), gradi di
  libertà, diagnosi di sovravincolo e conflitto, chiavi stabili, motore su libslvs.
  Fuori: scomposizione a grafo, matrici sparse, disuguaglianze. Chiusura: casi
  risolti a mano e confronto con un secondo risolutore sugli stessi schizzi.
- **0.4, `naming`.** Dentro: `StableId` deterministico, grafo di genealogia,
  serializzazione, risoluzione di un riferimento dopo una modifica. Chiusura:
  sequenze sintetiche di operazioni, compreso il salto delle facce, risolte in modo
  stabile.
- **0.5, `kernel`.** Dentro: facciata su OCCT con forme opache, operazioni con
  risultato e diagnosi, controllo di validità, strategie di ripiego, STEP in
  lettura e scrittura, fuzzing dell'import. Chiusura: archivio dei casi con valori
  attesi indipendenti e primo rapporto di robustezza.
- **0.6.** Le operazioni del kernel emettono la genealogia che `naming` usa.
  Chiusura: modificando a monte una quota, i riferimenti a facce e spigoli
  restano validi.
- **0.7, `features`.** Dentro: grafo delle feature (schizzo, estrusione, raccordo),
  rigenerazione, riferimenti per `StableId`. Chiusura: una parte con schizzo
  vincolato, estrusione e raccordo si rigenera dopo la modifica di una quota.
- **1.0.** Prestazioni, copertura, benchmark rieseguibile da chiunque.

## Regole delle tappe

### Stati

- **In programma**: esiste un contenuto previsto. Non si scrive codice.
- **Aperta**: la scheda è pubblicata con l'elenco dei pacchetti. È l'unica tappa su
  cui si lavora.
- **Chiusa**: tutti i criteri sono verificati. Il documento della tappa diventa
  storia e non si modifica.

### Aprire una tappa

Solo il responsabile, e solo dopo aver chiuso la precedente: trasforma il
contenuto previsto in scheda (`docs/tappe/X.Y-nome.md`), scrive e accetta gli ADR
che servono, elenca i pacchetti e aggiorna la tabella.

### Chiudere una tappa

Tutti i pacchetti sono uniti, tutti i criteri verificati uno per uno, i test
passano sui tre sistemi operativi senza avvisi. Si aggiorna la tabella e si apre la
tappa successiva.

### Cosa si può fare fuori dalla tappa aperta

- correggere errori delle tappe chiuse;
- migliorare documentazione e test;
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
