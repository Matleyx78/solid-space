# [0.3-PS8] `geom2d`: punto più vicino dell'ellisse

- **Tappa:** 0.3 — [piano](../0.3-regioni-ed-ellisse.md)
- **Issue:** #252
- **Difficoltà:** media
- **Agente:** claude
- **Modello e impegno:** medio; impegno alto. La revisione di Codex è mirata,
  in fascia forte, sull'errore garantito vicino all'evoluta e agli estremi
- **Riferimenti:** issue `[0.3-RS8]` (#251), agente codex, fascia forte
- **Contratto:** `geom2d/include/solid_space/geom2d/ellipse.hpp` (`nearest`,
  `EllipseNearest`, `kRelativeNearestError`)
- **Dipende da:** PS3 (#238), PS4 (#241); RS8 (#251). La revisione del contratto
  `[0.3-CS]` (#222) arriva attraverso PS4
- **Sblocca in Tecnigrafo:** PT50 ([Matleyx78/tecnigrafo#744](https://github.com/Matleyx78/tecnigrafo/issues/744)), con PS4 e PS7: lo
  snap *Vicino* e la scelta di un'ellisse con un clic
- **Componente:** `geom2d`
- **Da leggere:** [ADR 0017](../../adr/0017-ellisse-nel-piano.md) (Decisione,
  punti 5, 8 e 9; «Che cosa rende possibili decisioni esatte», il punto più
  vicino; il rischio in «Conseguenze»); [ADR 0013](../../adr/0013-esatto-dove-serve.md),
  punto 4 (la fascia); `ellipse.hpp`; le schede [PS3](PS3-polinomi.md) e
  [PS4](PS4-ellisse.md) e le loro intestazioni `exact/polynomial.hpp` ed
  `ellipse_meaning.hpp`; come documentazione, non come codice, il metodo di
  D. Eberly citato dall'ADR

## File

- `geom2d/src/ellipse_nearest.cpp` (nuovo)
- `tests/geom2d/ellipse_nearest_test.cpp` (nuovo)
- `tests/corpus/esegui/op_ellipse_nearest.cpp` (nuovo), con le letture di
  `ellipse_data.hpp` di PS4
- `tests/corpus/in-attesa/`: cancellare `0.3-RS8.txt`

Nessun `CMakeLists.txt`.

## Cosa fare

Realizzare, senza cambiarla, `nearest` di `ellipse.hpp`:

```cpp
inline constexpr double kRelativeNearestError = 0x1p-44;
[[nodiscard]] Result<EllipseNearest> nearest(const Ellipse& ellipse, Point2 p);
```

- **I candidati:** gli estremi dell'arco (quelli di `endpoints`, bit per bit) e
  i piedi delle perpendicolari da p (intersezioni dell'ellisse con l'iperbole
  di Apollonio di p, un polinomio di quarto grado); mai il prolungamento.
- **Il metodo è libero** (ADR 0017): per esempio la bisezione nel primo
  quadrante del metodo di Eberly, senza trigonometria, e `exact/polynomial.hpp`
  dove due piedi si confondono (vicino all'evoluta). Il contratto fissa che
  cosa si garantisce, non il metodo.
- **Le tre garanzie** dell'intestazione, con N la più grande fra M, |p.x| e
  |p.y|: distanza del candidato entro `kRelativeNearestError · N` dalla minima;
  punto e distanza entro lo stesso errore dal candidato. Il commento in testa a
  `ellipse_nearest.cpp` porta l'analisi dell'errore, anche vicino all'evoluta.
- **La scelta nella fascia** (ADR 0013, punto 4): deterministica, non fissata
  dal contratto; fuori dalla fascia il candidato è determinato. Un estremo
  scelto dà `atStart` o `atEnd` e il punto di `endpoints`; un'ellisse intera dà
  sempre `inside`.
- Diagnosi nell'ordine dell'intestazione (ellisse dato 0, p dato 1).

### Casi di verifica (per RS8)

| Operazione | `ingresso` | `atteso` |
|---|---|---|
| `nearest` | `ellisse`, `punto` | `punto` (approssimato), `distanza` (approssimata), `posizione` (nome di `Location`); oppure, nei casi nella fascia, solo le chiavi che valgono per ogni scelta (vedi sotto); oppure `diagnosi` (`dato` 0 o 1) |

L'ellisse ha la forma di [PS4](PS4-ellisse.md). Punto e distanza con
tolleranza `kRelativeNearestError · N`, per eccesso. Dove un caso cade nella
fascia, il riferimento non scrive il punto: scrive le proprietà che valgono per
ogni scelta (ADR 0017, «Verifica»), con chiavi che l'operazione calcola dal
risultato: `distanza` (uguale per tutti i candidati entro l'errore),
`sulla_curva` (vero se il punto dista dall'ellisse portante al più l'errore,
controllato con l'equazione implicita esatta), `posizione`.

**RS8** (fascia forte) copre almeno, alle due scale dell'ADR 0009:

- #217, caso 3b (punto oltre gli estremi di un arco: l'estremo, uguale bit per
  bit a `endpoints`) e caso 4 (rapporto 0,01, punti sull'asse maggiore fuori,
  dentro a meno del raggio di curvatura e dentro più lontano: due piedi
  simmetrici, nella fascia);
- punti fuori, dentro, sull'ellisse, al centro (ellisse di rapporto 1: ogni
  punto è un candidato, fascia), sugli assi, vicino all'evoluta, vicino agli
  estremi di un arco (un estremo e un piede quasi alla stessa distanza);
- le diagnosi per l'ellisse e per il punto.

Valori attesi con `decimal` ad alta precisione (radici del quarto grado
isolate con Sturm in razionali e raffinate); `provenienza` dice il procedimento.

## Regole della tappa che valgono qui

- **Contratto:** `ellipse.hpp` non cambia, commenti compresi. Se qualcosa manca
  o non torna, fermati come dice AGENTS.md.
- **Gli interni** della 0.2, di PS3 e di PS4 si usano senza cambiarli.
- **Determinismo** (ADR 0008, punto 5): calcolo nei `.cpp`; nessuna funzione
  trigonometrica (né di `<cmath>` né di `robust` oltre a `sinCos` già in PS4).
- File comuni: nessuno; cancelli solo il file dei tuoi riferimenti.

## Test richiesti

- `ellipse_nearest_test.cpp`, su dati generati in modo deterministico: la
  distanza di `nearest` non supera quella di nessun punto campionato sulla
  curva più l'errore; un punto di `nearest` su un'ellisse intera non è mai
  `atStart` né `atEnd`; un estremo scelto ha i bit di `endpoints`; punti vicino
  all'evoluta di ellissi con rapporti 1, 0,5, 0,01 e 1e-6.
- I casi di RS8 passano.

## Fatto quando

- `nearest` è realizzata come dichiara `ellipse.hpp`, con l'analisi dell'errore
  nel commento.
- I casi di `[0.3-RS8]` passano e il loro file è cancellato da `in-attesa/`.
- Il job del determinismo è verde.
- La CI è verde.
