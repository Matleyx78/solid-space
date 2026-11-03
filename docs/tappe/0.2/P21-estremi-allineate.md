# [0.2-P21] `geom2d`: estremi delle linee allineate unite

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** da aprire con l'unione della pull request del contratto (#159)
- **Difficoltà:** media
- **Agente:** claude
- **Modello e impegno:** medio; impegno alto
- **Riferimenti:** issue `[0.2-R21]`, agente codex (pareggi esatti delle
  proiezioni); scheda [R21](R21-estremi-allineate.md)
- **Contratto:** `geom2d/include/solid_space/geom2d/coincidence.hpp`
  (`joinAligned`)
- **Dipende da:** P8 (#49), R21
- **Sblocca in Tecnigrafo:** Matleyx78/tecnigrafo#373 (P22, l'unione, con P4 e
  P8): la linea unita di `JOIN` viene da qui
- **Componente:** `geom2d`
- **Da leggere:** [ADR 0009](../../adr/0009-regola-delle-tolleranze-del-piano.md)
  (riga **Corretto:** del 2026-11-02, punto 5 e Contratto);
  `coincidence.hpp` e `curves.hpp` («Checking the data»);
  `geom2d/src/coincidence.cpp` (P8) e `geom2d/src/exact/` (P6), come esempio di
  confronto esatto; la decisione #158 in [DECISIONI](../../DECISIONI.md)

## File

- `geom2d/src/coincidence.cpp`
- `tests/geom2d/coincidence_test.cpp`
- `tests/geom2d/contract_test.cpp` (la firma di `joinAligned`)
- `tests/corpus/esegui/op_coincidence.cpp` (l'operazione `joinAligned`)
- `tests/corpus/in-attesa/` (cancellare `0.2-R21.txt`)

## Cosa fare

Realizzare, senza cambiarla, `joinAligned` di `coincidence.hpp` (decisione del
responsabile del 2026-11-02, issue #158):

```cpp
[[nodiscard]] Result<Segment> joinAligned(const Segment& source, std::span<const Segment> others);
```

- **Gli estremi**, in quest'ordine: inizio e fine di `source`, poi inizio e
  fine di ogni segmento di `others`, nell'ordine di `others`.
- **Lungo la sorgente**, con d = `source.end − source.start`, un estremo p
  viene prima di q quando (p − `source.start`) · d è minore, deciso
  esattamente con i numeri di P6 (differenze e prodotti di `double`, senza
  arrotondare né dividere).
- **Il risultato** va dal primo estremo lungo la sorgente all'ultimo; a parità
  esatta vince il primo nell'ordine sopra, sia per l'inizio sia per la fine.
  I due punti sono `double` degli ingressi, restituiti senza cambiarli.
- **Nessuna tolleranza** e nessun controllo dell'allineamento: la funzione
  non prende `TolerancePolicy`.
- **Diagnosi** come dice l'intestazione: `invalidValue`, `outOfDomain` o
  `degenerateCurve` su `source` (dato 0) o su `others[i]` (dato i + 1),
  nell'ordine dei dati di `curves.hpp`; un solo segmento sbagliato fa fallire
  la chiamata.

### Casi di verifica (per R21)

| Operazione | `ingresso` | `atteso` |
|---|---|---|
| `joinAligned` | `sorgente` (un `segmento`), `altri` (una lista di `segmento`, anche vuota) | `curva` (un `segmento`), oppure `diagnosi` |

Il confronto è bit per bit: i punti attesi sono estremi degli ingressi.

## Regole della tappa che valgono qui

- **Contratto:** `coincidence.hpp` non cambia, commenti compresi. Se qualcosa
  manca o non torna, fermati e scrivilo nella issue (etichetta `contratto`).
- **Determinismo** (ADR 0008, punto 5): calcolo nei `.cpp`, nessuna
  trigonometria; qui servono solo confronti esatti.
- Le intestazioni interne di P6 e P7 si usano, non si cambiano.
- File comuni: nessuno; cancelli solo il file dei tuoi riferimenti in
  `tests/corpus/in-attesa/`. Nessun `CMakeLists.txt`.

## Test richiesti

- L'esempio della #158, in millimetri: sorgente (0,0)→(10,0), altro
  (0,5e-7)→(5,0); risultato (0,0)→(10,0), perché (0,0) e (0,5e-7) hanno la
  stessa proiezione esatta e vince la sorgente.
- Pareggio tra altri: sorgente (0,0)→(10,0), altri (12,1e-7)→(20,0) e
  (20,2e-7)→(15,0); risultato (0,0)→(20,0), e (0,0)→(20,2e-7) con gli altri
  in ordine inverso.
- Spazi e prolungamenti prima dell'inizio: altro (20,0)→(30,0) dà
  (0,0)→(30,0); altro (−5,0)→(−1,0) dà (−5,0)→(10,0); nessun altro dà la
  sorgente.
- Su dati generati in modo deterministico: invertire la sorgente dà lo stesso
  risultato invertito, bit per bit; il risultato non è mai degenere; i suoi
  punti sono sempre estremi degli ingressi.
- Le diagnosi, con il dato giusto e nell'ordine dei dati.
- I casi di R21 passano.

## Fatto quando

- `joinAligned` è realizzata come dichiara `coincidence.hpp`, con confronti
  esatti.
- I casi di `[0.2-R21]` passano e il loro file è cancellato da `in-attesa/`.
- Il job del determinismo è verde.
- La CI è verde.
