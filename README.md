# solid-space

> **Next-Generation Open-Source 3D Solid Modeling Kernel & Variational Constraint Solver**

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Language: C++](https://img.shields.io/badge/Language-C%2B%2B-orange.svg)]()
[![Architecture: Modular](https://img.shields.io/badge/Architecture-Modular%20Layered-green.svg)]()

**solid-space** è un progetto di ricerca e ingegneria software volto a sviluppare un kernel di modellazione solida B-Rep (*Boundary Representation*) e un risolutore geometrico variazionale libero di nuova generazione. 

L'obiettivo è colmare il divario strutturale tra gli standard proprietari di fascia industriale (**Siemens Parasolid + D-Cubed**, **Spatial ACIS**) e le soluzioni open-source esistenti, superando le fragilità storiche della geometria computazionale aperta[cite: 2, 4].

---

## 🎯 Requisiti Non Negoziabili

1. **Robustezza Deterministica delle Operazioni Booleane e di Raccordo:** Eliminazione dei fallimenti di cucitura e dell'inflazione cumulativa delle tolleranze.
2. **Topological Naming Manager (TNM) Nativo:** Tracciamento persistente della genealogia delle entità per scongiurare il *face flipping* nelle rigenerazioni parametriche.
3. **Solver Variazionale 2D/3D Scalabile:** Risoluzione simultanea a matrice sparsa guidata da decomposizione a grafo per grandi assiemi meccanici[cite: 2, 7].

---

## 🏛️ Architettura di Sistema a 4 Livelli

Il kernel è organizzato in livelli rigorosamente disaccoppiati per garantire modularità, testabilità e isolamento delle astrazioni[cite: 2]:

```text
┌─────────────────────────────────────────────────────────────────┐
│ LIVELLO 4: Application & Feature Engine (CAD / History Tree)    │
├─────────────────────────────────────────────────────────────────┤
│ LIVELLO 3: Bridge & Topological Naming Manager (TNM)             │
│   • Grafo di Genealogia Topologica (Lineage & Provenance)       │
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
│ LIVELLO 0: Substrato Geometrico & Interscambio (HAL)             │
│   • Astrazione Curve/Superfici analitiche e NURBS               │
│   • Parser formati industriali neutri (STEP AP203/214/242, IGES)│
└─────────────────────────────────────────────────────────────────┘
