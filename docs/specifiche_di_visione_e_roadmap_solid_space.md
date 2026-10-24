# solid-space: Visione Architetturale e Roadmap di Sviluppo

Documento guida e manifesto tecnico per la realizzazione di un kernel B-Rep 3D e un solver geometrico variazionale libero di classe industriale.

---

## 1. Visione d'Insieme e Obiettivo del Progetto

Il progetto **`solid-space`** si prefigge di colmare il divario storico tra i motori geometrici proprietari di riferimento industriale (**Siemens Parasolid + Siemens D-Cubed**, **Spatial ACIS + CDS**) e il panorama open-source odierno (frammentato tra Open CASCADE Technology per la modellazione solida e solver isolati come SolveSpace o FreeCAD GCS).

L'obiettivo non è realizzare un semplice wrapper, bensì sviluppare un **kernel di modellazione solida B-Rep 3D e un solver geometrico variazionale libero (GNU GPL v3)** fondato su tre pilastri non negoziabili:
1. **Robustezza deterministica e predittiva:** Eliminazione dei fallimenti delle operazioni booleane e di raccordo dovuti all'inflazione incontrollata delle tolleranze numeriche floating-point.
2. **Topological Naming persistente:** Tracciamento deterministico della genealogia degli elementi B-Rep attraverso le rigenerazioni del feature tree parametrico.
3. **Solver variazionale scalabile (2D/3D):** Pre-elaborazione a grafi di vincoli e algebra sparsa per gestire assiemi complessi composti da migliaia di vincoli.

---

## 2. Struttura del Repository e Politica delle Dipendenze

Il repository pubblico `Matleyx78/solid-space` adotta la separazione rigorosa tra dipendenze esterne immutabili e codice sorgente attivo mediante sottomoduli Git:

```text
solid-space/
├── .gitmodules
├── .gitignore
├── docs/
│   ├── VISION_AND_ROADMAP.md         # Versione internazionale (EN)
│   └── VISION_AND_ROADMAP_IT.md      # Versione italiana (IT)
├── third_party/
│   ├── occt/                         # Git Submodule immutabile (Open CASCADE Technology, LGPL 2.1 w/ exception)
│   └── solvespace/                   # Git Submodule immutabile (SolveSpace/libslvs, GNU GPL v3)
└── kernel/                           # Codice sorgente attivo del progetto solid-space (GNU GPL v3)
    ├── include/
    │   └── solid_space/
    └── src/
```

* **Licenza del Progetto:** L'inclusione e il collegamento del codice del solver derivato da SolveSpace comportano l'adozione della licenza **GNU General Public License v3.0 (GPLv3)** per l'intero binario unificato del kernel `solid-space`.

---

## 3. Strategia di Sviluppo: Approccio Modulare a Tappe

Lo sviluppo interamente da zero (*greenfield*) è stato scartato: riscrivere tre decenni di algoritmi di calcolo geometrico (curve e superfici analitiche, B-Spline, NURBS, algoritmi SSI e parser di interscambio STEP AP203/214/242 e IGES) richiederebbe tempi proibitivi e comporterebbe un rischio ingegneristico eccessivo.

La strategia adottata prevede:
* **OCCT come Layer 0 (Substrato Geometrico di Base):** Isolato dietro un'interfaccia astratta (HAL/Facade), OCCT fornisce la matematica delle curve/superfici e l'infrastruttura di I/O per i formati neutri STEP e IGES.
* **Riscrittura mirata dei componenti critici:** Sostituzione progressiva dei moduli di OCCT responsabili delle instabilità topologiche (il package booleano `BOPAlgo`, il gestore locale di tolleranze e il modulo di raccordo `BRepFilletAPI`).
* **Integrazione a monte:** Innesto di un motore di Topological Naming e di un solver variazionale a scomposizione a grafo.

---

## 4. I Pilastri Tecnologici

```text
┌─────────────────────────────────────────────────────────────────┐
│ LIVELLO 4: Application & Feature Engine (CAD / History Tree)    │
├─────────────────────────────────────────────────────────────────┤
│ LIVELLO 3: Bridge & Topological Naming Manager (TNM)             │
│   • Grafo di Genealogia Topologica (Lineage / Provenance)       │
│   • Mappatura vincoli solver ↔ riferimenti B-Rep stabili        │
├────────────────────────────────┬────────────────────────────────┤
│ LIVELLO 2A: Geometric Solver   │ LIVELLO 2B: Kernel B-Rep       │
│   • Graph-First (DRP)          │   • Cellular Topology          │
│   • Decomposizione matrici     │   • Exact Predicates / Snap    │
│   • Subsystem Gauss-Newton     │   • Bounded Tolerance Blending │
├────────────────────────────────┴────────────────────────────────┤
│ LIVELLO 1: Fondazioni Matematiche e Calcolo Numerico             │
│   • Algebra lineare sparsa (Eigen / SuiteSparse)                │
│   • Geometria Computazionale Robusta (Predicati esatti / CGAL) │
├─────────────────────────────────────────────────────────────────┤
│ LIVELLO 0: Substrato Geometrico e I/O (OCCT HAL / Facade)       │
│   • Geometria analitica e NURBS (Geom_Surface, Geom_Curve)      │
│   • Parser e serializzatori industriali (STEP AP242, IGES)      │
└─────────────────────────────────────────────────────────────────┘
```

### A. Riprogettazione del Kernel B-Rep (Stile Parasolid / ACIS)

1. **Eliminazione dell'inflazione cumulativa delle tolleranze:**
   * OCCT memorizza tolleranze locali scalari che vengono espanse a ogni operazione imperfetta, provocando collisioni spurie (*cascading tolerance failure*).
   * `solid-space` adotta un modello a tolleranza controllata rigida (*Bounded Tolerance Modeling*) basato su **predicati geometrici adattivi a precisione esatta** (stile Jonathan Shewchuk) per decisioni topologiche discrete invarianti rispetto agli errori IEEE 754.
   * Adozione di **snap-rounding deterministico**: due geometrie entro la soglia $\varepsilon$ vengono collassate topologicamente; in caso contrario rimangono rigorosamente distinte senza espansione delle sfere di tolleranza adiacenti.

2. **Topologia Cellulare (Cellular B-Rep):**
   * Superamento del modello classico a intersezione, split e cucitura (`stitching`), suscettibile di fallimenti in presenza di tangenze imperfette o contatti radenti (*grazing contacts*).
   * Partizione preventiva dello spazio solido e vuoto in complessi di celle chiuse e connesse disgiunte.
   * Le operazioni booleane si trasformano in una selezione combinatoria di insiemi di celle (*cell selection*), eliminando nativamente l'insorgenza di gusci aperti (*open shells*) e supportando modelli non-manifold e multi-materiale.

3. **Struttura Coedge / Half-Edge esplicita:**
   * Rimpiazzo del sistema indiretto a flag di orientamento (`TopAbs_Orientation`) con una topologia a puntatori bidirezionali a Coedge/Half-edge (modello ACIS e Parasolid *fin*).
   * Navigazione locale istantanea tra facce adiacenti per algoritmi avanzati di raccordo, smusso e riparazione dei bordi.

4. **Motore di Raccordatura Avanzata (Advanced Blending Engine):**
   * Algoritmo *Rolling Ball* con gestione analitica delle singolarità:
     * *Cliff Edges:* estrapolazione virtuale della superficie tangente prima del ritaglio sulla faccia contigua.
     * *Swallowtail / Cuspidi:* ritaglio automatico delle auto-intersezioni dei canali di raccordo quando il raggio $R$ supera la curvatura locale $1 / \kappa$.
     * *Vertex Blends complessi:* sintesi di superfici di chiusura ad $n$ lati (superfici di Gregory o patch B-Spline multipatch) con continuità $G^1$ e $G^2$ su vertici in cui convergono tre o più spigoli.

---

### B. Risolutore Geometrico Scalabile (Evoluzione libslvs in stile D-Cubed)

1. **Approccio "Graph-First" (Decomposition-Recombination Planning - DRP):**
   * Superamento del limite di SolveSpace, che aggrega tutti i vincoli in una singola matrice Jacobiana densa globale, degradando oltre poche centinaia di gradi di libertà.
   * Pre-elaborazione basata sulla teoria dei grafi per identificare cluster cinematici rigidi (*rigid bodies*) e isolare sottosistemi indipendenti, risolvendo numericamente solo le interconnessioni residue.

2. **Algebra Lineare Sparsa:**
   * Adozione di solutori specializzati per matrici sparse (SuiteSparse, Eigen Sparse), riducendo la complessità asintotica da $\mathcal{O}(n^3)$ a regimi quasi-lineari.

3. **Supporto a Vincoli di Disuguaglianza:**
   * Estensione del solutore oltre i vincoli di sola uguaglianza $F(\mathbf{x}) = 0$.
   * Formulazione basata su Sequential Quadratic Programming (SQP) per gestire quote dimensionali con tolleranza d'intervallo, corse minime/massime e limiti angolari cinematici ($g(\mathbf{x}) \le 0$).

---

### C. Topological Naming Manager (TNM) Nativo

1. **Lineage Tracking persistente:**
   * Risoluzione strutturale del problema del salto delle facce (*face flipping*) e della rottura dei vincoli durante la modifica di quote a monte nel feature tree.
   * Ogni operazione topologica emette una funzione di genealogia deterministica:
     $$\text{ID}_{\text{nuovo}} = \mathcal{H}(\text{OpType}, \text{ID}_{\text{genitore\_1}}, \text{ID}_{\text{genitore\_2}}, \text{Ruolo})$$

2. **Disaccoppiamento dai puntatori di memoria:**
   * Gli identificatori dei vincoli dello sketcher e dell'assieme non puntano mai a indirizzi fisici volatili di memoria (come i puntatori C++ a `TopoDS_Face`), ma unicamente alle chiavi stabili del grafo genealogico topologico.

---

## 5. Roadmap Operativa a Fasi

| Fase | Denominazione | Obiettivi Chiave | Risultato Atteso |
| :--- | :--- | :--- | :--- |
| **Fase 1** | **Topological Naming & Bridge Dati** | Specificare il grafo di genealogia topologica indipendente da OCCT; creare il wrapper astratto tra entità stabili e primitive del solver. | Mappatura vincoli ↔ ID topologici persistente priva di riferimenti di memoria volatili. |
| **Fase 2** | **Decomposizione a Grafo del Solver** | Estrarre il motore matematico di SolveSpace; rimuovere il solutore monolitico denso; implementare algoritmi di clustering DRP e supporto matrici sparse. | Solver 2D/3D scalabile su assiemi complessi con supporto per range di disuguaglianza. |
| **Fase 3** | **Predicati Esatti & Booleana Cellulare** | Integrare predicati geometrici esatti (Shewchuk); sviluppare il partizionatore spaziale cellulare e l'operatore booleano combinatorio su volumi poliedrici e superfici NURBS. | Operazioni booleane deterministiche immuni da gusci aperti e inflazione di tolleranze. |
| **Fase 4** | **Integrazione di Sistema & Benchmark** | Unificare kernel cellulare, TNM e solver variazionale; implementare patch di Gregory per blend complessi; validazione su benchmark industriali. | Parità architetturale con gli standard commerciali verificata su test NIST e cinematici. |