# [0.2-P22] `geom2d`: parte di ciascuna curva tenuta dal raccordo

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** #190
- **Difficoltà:** impegnativa
- **Agente:** claude
- **Modello e impegno:** forte; impegno alto
- **Riferimenti:** issue `[0.2-R22]`, agente codex (segni esatti su numeri
  algebrici); scheda [R22](R22-parte-tenuta-raccordo.md)
- **Contratto:** `geom2d/include/solid_space/geom2d/tangency.hpp`
  (`KeptPart`, `Fillet::keepFirst` e `Fillet::keepSecond`, regola 5 di
  `fillet`, `TangencyOutcome::ambiguousPick` e `radiusTooLarge`)
- **Dipende da:** P16 (#70), P20 (#145), R22 (#189)
- **Sblocca in Tecnigrafo:** Matleyx78/tecnigrafo#367 (P19, raccordi, con
  P4–P10, P12, P15 e P16): le curve si tagliano e si prolungano con la parte
  tenuta, senza decisioni geometriche di Tecnigrafo
- **Componente:** `geom2d`
- **Da leggere:** [ADR 0011](../../adr/0011-raccordi-e-tangenze-nel-piano.md)
  (riga **Corretto:** del 2026-11-04); `tangency.hpp` (`KeptPart`, `Fillet`,
  `fillet` e le sue cinque regole), `side.hpp`, `intersection.hpp`; la
  decisione #184 in [DECISIONI](../../DECISIONI.md); `geom2d/src/fillet.cpp`
  (P16, P20), `tangency_internal.hpp` (P15) ed `exact/` (P6, P12), dove sono
  già decisi lati, verso dell'arco e posizioni

## File

- `geom2d/src/fillet.cpp`
- `geom2d/src/tangency_internal.hpp` (solo aggiunte, se servono)
- `tests/geom2d/fillet_test.cpp`
- `tests/geom2d/contract_test.cpp` (`KeptPart` e i due campi)
- `tests/corpus/esegui/op_fillet.cpp` (le chiavi `parte_prima` e
  `parte_seconda`)
- `tests/corpus/in-attesa/` (cancellare `0.2-R22.txt`)

## Cosa fare

Realizzare, senza cambiarla, la regola 5 di `fillet` in `tangency.hpp`
(decisione del responsabile del 2026-11-04, issue #184):

```cpp
enum class KeptPart : std::uint8_t { fromStart, toEnd };
// in Fillet:
KeptPart keepFirst = KeptPart::fromStart;
KeptPart keepSecond = KeptPart::fromStart;
```

- **Con un arco da proseguire** (regole 1 e 3, punti di tangenza esatti
  distinti): con t la direzione della curva nel suo punto di tangenza e a la
  direzione in cui l'arco esatto lascia quel punto verso l'altro, `toEnd`
  quando dot(t, a) < 0, `fromStart` quando dot(t, a) > 0. t e a sono
  paralleli, quindi il segno discende da decisioni già esatte: il lato del
  cerchio rispetto alla curva (`sideOfFirst`, `sideOfSecond` del candidato), il
  verso dell'arco (regola 4) e, per una curva circolare, il suo verso. Nessun
  valore arrotondato decide il segno.
- Se una curva dritta dovrebbe tornare indietro in T, ossia
  `beforeStart + fromStart` o `afterEnd + toEnd` con un arco da
  proseguire, esito `radiusTooLarge` senza geometria (#187).
- **Senza un arco da proseguire**, usare il segno esatto di
  dot(pick − P, t) per rette (CHAMFER) e cerchi interi (lato locale).
  Per archi e segmenti curvi, usare l'appartenenza esatta della
  direzione del clic alle parti lungo il sostegno. Se P è fuori arco
  ed entrambe contengono il clic, scegliere il prolungamento angolare
  minore con confronto esatto, o dare `ambiguousPick` in parità (#188).
  La regola 5 segue la scelta del candidato e le diagnosi di dominio.
- La simmetria vale a geometria scelta uguale. Restano le precedenze
  dei semicerchi nelle regole 1 e 4 (#186).

### Casi di verifica (per R22)

| Operazione | `ingresso` | `atteso`, in più |
|---|---|---|
| `fillet` | come in R16a–R16c | `parte_prima`, `parte_seconda`: `fromStart` oppure `toEnd` (solo con `esito` `found`) |

## Regole della tappa che valgono qui

- **Contratto:** `tangency.hpp` non cambia, commenti compresi. Se qualcosa
  manca o non torna, fermati e scrivilo nella issue (etichetta `contratto`).
- **Determinismo** (ADR 0008, punto 5): calcolo nei `.cpp`; qui servono solo
  segni esatti.
- Le decisioni esatte di `exact/` si usano, non si cambiano.
- File comuni: `tangency_internal.hpp` solo con aggiunte; cancelli solo il
  file dei tuoi riferimenti in `tests/corpus/in-attesa/`. Nessun
  `CMakeLists.txt`.

## Test richiesti

- Gli esempi della #184: L1 (0,0)→(30,0) scelta in (5,0), L2 (0,0)→(0,30)
  scelta in (0,5), raggio 10: `toEnd` e `toEnd`; il caso e di
  `autocad-raccordi.dxf` (arco e linea, raggio 3): `toEnd` e `toEnd`.
- Su dati generati in modo deterministico, per ogni coppia di tipi,
  a geometria scelta uguale: invertire una curva scambia il suo
  valore e lascia l'altro; scambiare le curve scambia i due campi.
  Con un arco da proseguire il percorso non torna indietro.
- Gli esempi #186, #187 e #188 della scheda R22: precedenze dei due
  semicerchi, `radiusTooLarge` senza geometria, estensione dell'arco
  nei due versi e parità esatta.
- Senza arco di raccordo, su rette: clic sulla normale
  (`ambiguousPick`) e a un ulp da essa. `outOfDomain` ha precedenza
  sulla scelta della parte.
- I casi di R22 passano, e quelli già nell'archivio per `fillet` passano
  ancora.

## Fatto quando

- La regola 5 è realizzata come dichiara `tangency.hpp`, con segni esatti.
- I casi di `[0.2-R22]` passano e il loro file è cancellato da `in-attesa/`.
- Il job del determinismo è verde.
- La CI è verde.
