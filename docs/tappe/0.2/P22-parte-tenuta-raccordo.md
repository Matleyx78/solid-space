# [0.2-P22] `geom2d`: parte di ciascuna curva tenuta dal raccordo

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** da aprire
- **Difficoltà:** impegnativa
- **Agente:** claude
- **Modello e impegno:** forte; impegno alto
- **Riferimenti:** issue `[0.2-R22]`, agente codex (segni esatti su numeri
  algebrici); scheda [R22](R22-parte-tenuta-raccordo.md)
- **Contratto:** `geom2d/include/solid_space/geom2d/tangency.hpp`
  (`KeptPart`, `Fillet::keepFirst` e `Fillet::keepSecond`, regola 5 di
  `fillet`, `TangencyOutcome::ambiguousPick`)
- **Dipende da:** P16 (#70), P20 (#145), R22
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
- **Senza un arco da proseguire** (regola 2, raggio zero, e punti di tangenza
  esatti coincidenti): `toEnd` quando dot(pick − P, t) > 0, `fromStart` quando
  è < 0, con P lo spigolo esatto (il centro nella regola 2, il punto comune
  altrimenti) e pick il punto indicato della stessa curva. Il segno si decide
  esattamente, con P algebrico come lo dà la costruzione. Zero per una delle
  due curve: `ambiguousPick`, dopo la scelta del candidato e dopo ogni
  diagnosi di dominio.
- Per un `Circle`, t è la direzione antioraria; per un segmento di polilinea
  curvo, quella da `start` a `end` lungo il suo arco.
- Le altre parti di `fillet` non cambiano: candidati, scelta, diagnosi, errori.

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
- Su dati generati in modo deterministico, per ogni coppia di tipi: invertire
  una curva scambia il suo valore e lascia l'altro; scambiare le curve, quando
  il raccordo è lo stesso (non la regola 1), scambia `keepFirst` e
  `keepSecond`; dove la parte tenuta contiene un tratto della curva data, il
  percorso parte tenuta, arco, parte tenuta non torna indietro nei punti di
  tangenza.
- Regola 2 e punti coincidenti: la parte dalla parte del punto indicato; il
  punto indicato sulla normale (`ambiguousPick`) e a un ulp da essa;
  `outOfDomain` prima di `ambiguousPick`.
- I casi di R22 passano, e quelli già nell'archivio per `fillet` passano
  ancora.

## Fatto quando

- La regola 5 è realizzata come dichiara `tangency.hpp`, con segni esatti.
- I casi di `[0.2-R22]` passano e il loro file è cancellato da `in-attesa/`.
- Il job del determinismo è verde.
- La CI è verde.
