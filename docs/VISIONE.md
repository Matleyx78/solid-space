# solid-space: visione

Perché esiste solid-space, che cosa promette e che cosa lascia per dopo. Le
decisioni definitive sono negli [ADR](adr/); qui c'è il perché.

## 1. Scopo

solid-space è il **kernel geometrico di Tecnigrafo**, il CAD libero 2D e 3D dello
stesso responsabile. Non è un prodotto a sé che compete con i motori industriali:
è la libreria su cui Tecnigrafo costruirà i vincoli parametrici e, nella sua fase
2, il modellatore 3D.

Il primo manifesto del progetto puntava a colmare il divario con **Parasolid + D-Cubed**
e **ACIS**, riscrivendo booleane, raccordi e gestione delle tolleranze. L'analisi
fatta in Tecnigrafo (`docs/VALUTAZIONI-INIZIALI.md`) dice il contrario: il vantaggio
dei motori commerciali sono decenni di casi limite risolti, non un algoritmo
segreto, e un piccolo gruppo non può rifarli. Può però fare tre cose, e farle bene.

## 2. I tre pilastri

### Solver variazionale

Risolve vincoli geometrici e dimensionali (coincidenza, parallelismo, tangenza,
quote), prima in 2D per il disegno e gli schizzi, poi in 3D per gli assiemi.

- L'interfaccia è nostra: entità, vincoli e chiavi stabili in ingresso; soluzione,
  gradi di libertà e diagnosi dei conflitti in uscita. Nessuna struttura di
  libslvs o di altri motori è visibile fuori dal componente.
- Si parte avvolgendo libslvs così com'è. Scomposizione a grafo e matrici sparse
  arrivano solo se un confronto misurato mostra che servono.

### Naming topologico stabile

È il problema che ha fatto soffrire di più i CAD liberi, e costa poco se si
progetta prima della prima estrusione e moltissimo se si aggiunge dopo.

Ogni operazione che crea o modifica elementi emette identificativi derivati in modo
deterministico da tipo di operazione, genitori e ruolo:

```text
ID_nuovo = H(tipo_operazione, ID_genitore_1, ID_genitore_2, ruolo)
```

Vincoli, schizzi e feature puntano **solo** a queste chiavi, mai a indirizzi di
memoria né a tipi di OCCT. Per il 2D la stessa idea si traduce nelle chiavi di
Tecnigrafo: handle dell'entità più ruolo (estremo, centro).

### Operazioni solide robuste

Booleane, raccordi, smussi, svuotamenti e import/export STEP, sopra OCCT. La
robustezza si ottiene con un **metodo**:

- ogni risultato è controllato prima di essere restituito;
- se un'operazione fallisce, si prova una sequenza di strategie alternative
  (riparazione della forma, tolleranza di fusione, ordine diverso, piccole
  variazioni) e si dice sempre quale è servita;
- l'utente riceve una diagnosi chiara di che cosa non è riuscito e dove;
- un archivio di casi difficili cresce a ogni difetto trovato, con valori attesi
  calcolati indipendentemente.

Le regole sono nell'[ADR 0004](adr/0004-robustezza-e-verifica.md).

## 3. Principi di lavoro

1. **Misurare prima di riscrivere.** Un modulo di OCCT si sostituisce solo con un
   ADR che porta i dati del nostro archivio di casi.
2. **A pezzi piccoli, nell'ordine in cui serve a Tecnigrafo.** I predicati
   robusti prima delle modifiche 2D, il solver prima dei vincoli, il resto prima
   della fase 2.
3. **OCCT dietro una facciata.** Fuori dal componente `kernel` nessuno vede tipi
   di OCCT. Se un giorno servirà sostituire un solo pezzo, o un altro nucleo, si
   potrà.
4. **Ogni componente si compila da solo**, con le dipendenze minime.
5. **Una sola direzione.** Le tappe le decide il responsabile; il codice è di
   tutti, la direzione no (stesso modello di Tecnigrafo).

## 4. Il rapporto con Tecnigrafo

| Quando in Tecnigrafo | Che cosa prende da solid-space |
|---|---|
| Tappa 0.7, comandi di modifica | predicati robusti (`robust`) per intersezioni, trim e offset |
| Tappa dei vincoli 2D | `solver` con chiavi handle più ruolo, senza OCCT |
| Fase 2, modellatore 3D | `kernel`, `naming`, `features` |

Come Tecnigrafo incorpora la libreria (sottomodulo, `FetchContent` o altro) si
decide con l'ADR di Tecnigrafo che introduce la dipendenza.

## 5. Appendice: Non ora

Le ambizioni del primo manifesto non sono respinte: sono **rimandate in attesa di
un dato**. Un'idea esce da questa tabella solo con un ADR che porta le misure
dell'archivio di casi e una soglia dichiarata in anticipo.

| Idea | Perché non ora | Che cosa la riporterebbe nel piano |
|---|---|---|
| Topologia cellulare: partizione preventiva dello spazio in celle, booleane come scelta di celle | È un nuovo kernel B-Rep, non un miglioramento: costo di ricerca molto alto | Una classe di casi di booleana che fallisce in modo non recuperabile con le strategie della tappa 0.5 |
| Struttura coedge/half-edge propria al posto di quella di OCCT | Costringerebbe a riscrivere tutto ciò che sta sopra `TopoDS` | Una navigazione topologica che con OCCT è dimostrabilmente impraticabile per un'operazione necessaria |
| Motore di raccordi proprio (rolling ball, cliff edge, cuspidi, vertex blend con patch di Gregory) | Il raccordo è il punto più debole di OCCT ma anche il più difficile da rifare | Archivio con raccordi che falliscono dopo tutte le strategie alternative, e una classe ben delimitata da coprire |
| Modellazione a tolleranza rigida, snap-rounding deterministico nelle booleane | Richiede di toccare il cuore delle operazioni di OCCT | Fallimenti ripetibili attribuiti all'inflazione delle tolleranze |
| Predicati esatti dentro le decisioni topologiche delle booleane | I predicati si fanno comunque (`robust`), ma il loro uso dentro le booleane implica un'operazione propria | Come sopra, per una booleana specializzata (per esempio sui poliedri) |
| Scomposizione a grafo dei vincoli (DRP) e algebra sparsa (Eigen, SuiteSparse) | Servono per migliaia di vincoli, cioè per gli assiemi | Un benchmark in cui il solver di partenza non scala sugli schizzi o sugli assiemi reali |
| Vincoli di disuguaglianza (SQP): corse minime e massime, limiti angolari | Servono per la cinematica degli assiemi | La tappa degli assiemi di Tecnigrafo |
| Un altro nucleo al posto di OCCT | La facciata lascia la porta aperta, ma oggi non c'è un'alternativa matura in C++ | Un confronto sull'archivio di casi che favorisca nettamente un'alternativa |
| Parità con Parasolid o ACIS come obiettivo dichiarato | Non è un obiettivo misurabile né realistico per un piccolo gruppo | Nessuna: resta fuori |
