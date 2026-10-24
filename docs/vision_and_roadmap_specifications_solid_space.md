# solid-space: Architectural Vision and Development Roadmap

Master design charter and technical specifications for building an industrial-grade, Free Software 3D B-Rep solid modeling kernel and variational geometric constraint solver.

---

## 1. Executive Summary & Project Vision

The **`solid-space`** project is engineered to bridge the long-standing divide between proprietary industrial standard platforms (**Siemens Parasolid + Siemens D-Cubed**, **Spatial ACIS + CDS**) and the fragmented open-source ecosystem (primarily limited to Open CASCADE Technology for B-Rep modeling and isolated solvers such as SolveSpace or FreeCAD GCS).

The ultimate goal is not to produce a thin wrapper, but to construct a **next-generation 3D B-Rep solid modeling kernel and scalable variational geometric constraint solver released under the GNU GPL v3 license**, founded upon three non-negotiable requirements:
1. **Deterministic and Predictive Robustness:** Complete eradication of boolean and filleting failures caused by cumulative floating-point tolerance inflation.
2. **Intrinsic Persistent Topological Naming:** Deterministic lineage tracking of all B-Rep entities across parametric feature tree regenerations.
3. **Scalable Variational Solver (2D/3D):** Graph-first constraint decomposition and sparse linear algebra designed to handle assemblies containing thousands of interdependent constraints.

---

## 2. Repository Layout & Dependency Architecture

The public repository `Matleyx78/solid-space` enforces strict isolation between external immutable upstream dependencies and active project source code via Git submodules:

```text
solid-space/
├── .gitmodules
├── .gitignore
├── docs/
│   ├── VISION_AND_ROADMAP.md         # International master specification (EN)
│   └── VISION_AND_ROADMAP_IT.md      # Italian specification (IT)
├── third_party/
│   ├── occt/                         # Immutable Git submodule (Open CASCADE Technology, LGPL 2.1 w/ exception)
│   └── solvespace/                   # Immutable Git submodule (SolveSpace/libslvs, GNU GPL v3)
└── kernel/                           # Active solid-space core engine (GNU GPL v3)
    ├── include/
    │   └── solid_space/
    └── src/
```

* **License Governance:** Because the geometric constraint solver incorporates and statically/dynamically links algorithms derived from SolveSpace, the unified `solid-space` kernel library is released under the **GNU General Public License v3.0 (GPLv3)**.

---

## 3. Development Strategy: Phased Modular Refactoring

A pure *greenfield* approach (rebuilding everything from scratch) was deliberately rejected: reimplementing over thirty years of foundational computational geometry (analytical curves and surfaces, B-Splines, NURBS, Surface-Surface Intersection algorithms, and ISO STEP AP203/214/242 / IGES readers and writers) carries unacceptable technical risk and timeline overhead.

The adopted path relies on a **Phased Modular Architecture**:
* **OCCT as Layer 0 (Abstract Geometric Substrate):** Encapsulated strictly behind an abstract Hardware/Software Abstraction Layer (HAL/Facade), OCCT is retained for foundational curve/surface mathematics and neutral STEP/IGES exchange formats.
* **Targeted Replacement of Fragile Subsystems:** High-risk failure points in OCCT (the `BOPAlgo` boolean package, cumulative tolerance inflation, and the `BRepFilletAPI` blending module) are systematically redesigned.
* **Upstream Enhancement:** Implementation of an intrinsic Topological Naming Manager and a graph-decomposed variational constraint engine.

---

## 4. Core Technological Pillars

```text
┌─────────────────────────────────────────────────────────────────┐
│ LAYER 4: Application & Feature Engine (CAD / History Tree)      │
├─────────────────────────────────────────────────────────────────┤
│ LAYER 3: Bridge & Topological Naming Manager (TNM)              │
│   • Topological Lineage & Provenance Graph                      │
│   • Solver Constraint ↔ Stable B-Rep Entity Mapping             │
├────────────────────────────────┬────────────────────────────────┤
│ LAYER 2A: Geometric Solver     │ LAYER 2B: B-Rep Kernel         │
│   • Graph-First (DRP)          │   • Cellular Topology          │
│   • Matrix Decomposition       │   • Exact Predicates / Snap    │
│   • Subsystem Gauss-Newton     │   • Bounded Tolerance Blending │
├────────────────────────────────┴────────────────────────────────┤
│ LAYER 1: Mathematical Foundations & Numerical Computing        │
│   • Sparse Linear Algebra (Eigen / SuiteSparse)                 │
│   • Robust Computational Geometry (Exact Predicates / CGAL)    │
├─────────────────────────────────────────────────────────────────┤
│ LAYER 0: Geometric Substrate & I/O (OCCT HAL / Facade)          │
│   • Analytical & NURBS Geometry (Geom_Surface, Geom_Curve)      │
│   • Industrial Transmit Parsers (STEP AP242, IGES)              │
└─────────────────────────────────────────────────────────────────┘
```

### A. Parasolid/ACIS-Class B-Rep Kernel Refactoring

1. **Elimination of Cumulative Tolerance Inflation:**
   * OCCT assigns local scalar tolerances to vertices, edges, and faces, expanding them on every imprecise operation and triggering cascading intersection failures.
   * `solid-space` introduces **Bounded Tolerance Modeling** governed by **adaptive exact geometric predicates** (Shewchuk-style) for all discrete topological orientation, coplanarity, and containment queries, rendering them immune to IEEE 754 precision loss.
   * **Deterministic Snap-Rounding:** Geometric entities within a defined threshold $\varepsilon$ are collapsed topologically; otherwise, they remain strictly separated without inflating surrounding tolerance boundaries.

2. **Cellular B-Rep Topology:**
   * Replaces the conventional intersection $\rightarrow$ split $\rightarrow$ stitching pipeline, which fails frequently on grazing contacts and near-tangencies.
   * Partitions solid and empty space into closed, connected, disjoint cell complexes.
   * Boolean operations (union, intersection, difference) are reduced to combinatorial **cell selection**, natively preventing open shell defects and providing first-class support for non-manifold and multi-material domains.

3. **Explicit Coedge / Half-Edge Topology:**
   * Replaces OCCT's orientation-flagged edge model (`TopAbs_Orientation`) with direct, bidirectional coedge/half-edge pointer structures (mirroring ACIS and Parasolid fins).
   * Enables $O(1)$ local topological traversal between neighboring faces, a prerequisite for advanced blending and topology repair routines.

4. **Advanced Blending Engine:**
   * Implements a rolling-ball solver with comprehensive singularity handling:
     * *Cliff Edges:* Virtual extrapolation of supporting tangent surfaces before boundary trimming.
     * *Swallowtail Cusps:* Analytical trimming of self-intersecting blend channels when radius $R$ exceeds local curvature $1 / \kappa$.
     * *Complex Vertex Blends:* Synthesis of $n$-sided filling patches (Gregory surfaces or multipatch B-Splines) maintaining $G^1$ and $G^2$ continuity across vertices where three or more edges converge.

---

### B. Scalable Variational Constraint Solver (D-Cubed Evolution of SolveSpace)

1. **Graph-First Constraint Preprocessing (Decomposition-Recombination Planning - DRP):**
   * Eliminates SolveSpace's bottleneck of assembling all sketch/assembly constraints into a single monolithic dense Jacobian matrix.
   * Applies bipartite graph analysis, connected-component matching, and block decomposition to detect rigid clusters, solving numerically only the active unconstrained degrees of freedom.

2. **Sparse Matrix Architecture:**
   * Integrates high-performance sparse linear solvers (SuiteSparse, Eigen Sparse), shifting system complexity from $\mathcal{O}(n^3)$ to quasi-linear performance on large assemblies.

3. **Inequality and Range Constraints:**
   * Extends the variational solver beyond rigid equality constraints $F(\mathbf{x}) = 0$.
   * Utilizes Sequential Quadratic Programming (SQP) to support bounded tolerances, minimum/maximum clearances, and kinematic actuator travel limits ($g(\mathbf{x}) \le 0$).

---

### C. Native Persistent Topological Naming Manager (TNM)

1. **Persistent Lineage Tracking:**
   * Solves the notorious face-flipping anomaly during feature tree recalculations.
   * Every primitive topological modifier emits a deterministic lineage signature:
     $$\text{ID}_{\text{new}} = \mathcal{H}(\text{OpType}, \text{ID}_{\text{parent\_1}}, \text{ID}_{\text{parent\_2}}, \text{Role})$$

2. **Decoupling from Ephemeral Memory Pointers:**
   * Sketcher constraints and assembly references never link to raw C++ memory pointers (such as transient `TopoDS_Face` addresses); they bind strictly to unique lineage keys within the provenance graph.

---

## 5. Phased Operational Roadmap

| Phase | Milestone | Key Technical Deliverables | Expected Outcome |
| :--- | :--- | :--- | :--- |
| **Phase 1** | **Topological Naming & Data Bridge** | Specify the kernel-independent topological lineage graph; implement an abstract interface bridging stable entities to solver primitives. | Persistent constraint-to-topology mapping independent of ephemeral memory pointers. |
| **Phase 2** | **Graph Decomposition Solver Engine** | Decouple SolveSpace's numerical solver core; eliminate monolithic dense resolution; integrate DRP graph decomposition and sparse matrix support. | Scalable 2D/3D variational solver handling complex assemblies and inequality range constraints. |
| **Phase 3** | **Exact Predicates & Cellular Booleans** | Integrate Shewchuk adaptive exact predicates; build spatial cellular decomposition engine and combinatorial cell-selection booleans. | Deterministic boolean operations immune to open-shell defects and tolerance inflation. |
| **Phase 4** | **System Integration & Industrial Benchmarks** | Unify cellular kernel, TNM, and variational solver; deploy Gregory corner patch synthesis; execute NIST and kinematic validation suites. | Industrial-grade validation against standard NIST B-Rep and redundant assembly test suites. |