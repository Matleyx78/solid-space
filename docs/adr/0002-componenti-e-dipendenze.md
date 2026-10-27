# ADR 0002: Componenti e dipendenze

- **Stato:** Proposto
- **Data:** 2026-10-27

## Contesto

Tecnigrafo avrà bisogno di pezzi diversi in momenti diversi: predicati robusti per
il 2D, un solver per i vincoli, e solo molto più tardi un kernel solido. Se
solid-space fosse un blocco unico, il CAD 2D dovrebbe compilare anche OCCT. Inoltre
OCCT e il motore del solver sono dipendenze grandi che vogliamo poter sostituire.

## Decisione

1. **Cinque componenti**, ciascuno un target CMake con alias
   `solid_space::<nome>` e un namespace `solid_space::<nome>`:

   | Componente | Dipende da |
   |---|---|
   | `robust` | solo libreria standard |
   | `naming` | solo libreria standard |
   | `solver` | `robust`, libslvs (privato) |
   | `kernel` | `robust`, `naming`, OCCT (privato) |
   | `features` | `kernel`, `naming`, `solver` |

2. **Ogni componente si compila da solo.** Con `kernel` e `features` spenti non
   serve OCCT. L'opzione CMake che li spegne si definisce nella tappa 0.1.
3. **Le dipendenze vanno in un solo verso.** Un componente non include uno che sta
   sopra di lui. Il grafo non ha cicli.
4. **Le intestazioni pubbliche non espongono dipendenze esterne.** Nessuna
   intestazione pubblica include OCCT, libslvs, Eigen o altre librerie. Le forme
   e le strutture del solver sono maniglie opache. Questo è il confine che
   permette di sostituire un modulo di OCCT, il motore del solver o l'intero
   nucleo.
5. **OCCT è il nucleo geometrico**, collegato solo da `kernel` e in modo privato.
   Si cerca prima come pacchetto di sistema; altrimenti si usa il sottomodulo
   `third_party/occt`, a una versione fissata. Con questa scelta il responsabile
   chiude la discussione #104 di Tecnigrafo (OCCT contro un nucleo proprio).
6. **libslvs (SolveSpace, GPLv3)** è il primo motore del solver, collegato solo
   da `solver`, dal sottomodulo `third_party/solvespace` non modificato. Il solver
   ha un'interfaccia nostra (entità, vincoli, chiavi stabili, diagnosi) e il motore
   resta sostituibile.
7. **L'albero delle feature sta in solid-space** (`features`), senza interfaccia
   grafica, così rigenerazione e naming si provano da soli. Comandi, interfaccia,
   annulla e ripeti e salvataggio nel `.cadx` restano a Tecnigrafo.
8. **Come Tecnigrafo incorpora la libreria** (sottomodulo, `FetchContent` o altro)
   non si decide qui: lo decide l'ADR di Tecnigrafo che introduce la dipendenza.
   Qui si garantisce solo il punto 2.
9. **Sottomoduli fissati e non modificati.** Le correzioni a OCCT o a SolveSpace si
   mandano a monte; una modifica locale richiede un ADR.

## Alternative considerate

- **Un'unica libreria monolitica**: costringe il 2D a compilare OCCT.
- **Il solver dentro `kernel`**: il solver serve prima e senza il kernel.
- **L'albero delle feature in Tecnigrafo**: renderebbe il naming difficile da
  provare senza l'applicazione.
- **Esporre i tipi di OCCT per risparmiare codice di traduzione**: rende il nucleo
  impossibile da sostituire e porta OCCT in tutta l'applicazione.

## Conseguenze

- Più target e più CMake da mantenere, in cambio di dipendenze minime per chi usa
  solo una parte.
- La facciata richiede codice di traduzione tra i tipi pubblici e quelli di OCCT:
  è il costo della sostituibilità e va tenuto sottile.
- Il confine con Tecnigrafo è l'insieme delle intestazioni pubbliche: cambiarle è
  una decisione condivisa dei due progetti.
