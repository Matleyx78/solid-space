# [0.3-PS4] `geom2d`: l'ellisse, i suoi estremi e il rettangolo

- **Tappa:** 0.3 — [piano](../0.3-regioni-ed-ellisse.md)
- **Issue:** #241
- **Difficoltà:** media
- **Agente:** codex
- **Modello e impegno:** medio; impegno alto. La revisione di Claude è mirata,
  in fascia forte, sulla regola dell'ampiezza nominale e sul rettangolo
- **Riferimenti:** issue `[0.3-RS4a]` (#239), agente claude, fascia forte;
  `[0.3-RS4b]` (#240), agente claude, fascia leggera
- **Contratto:** `geom2d/include/solid_space/geom2d/ellipse.hpp` (`Ellipse`,
  `magnitude`, `endpoints`, `meetsRectangle`)
- **Dipende da:** la revisione del contratto `[0.3-CS]` (#222); RS4a (#239),
  RS4b (#240)
- **Sblocca in Tecnigrafo:** PT50 (issue ancora da aprire), con PS7 e PS8: la
  finestra che attraversa usa `meetsRectangle`, lo snap *Estremo* `endpoints`
- **Componente:** `geom2d`
- **Da leggere:** [ADR 0017](../../adr/0017-ellisse-nel-piano.md) (Decisione,
  punti 1–4, 6 e 8; «Che cosa rende possibili decisioni esatte»); `ellipse.hpp`;
  `curves.hpp` (la regola di `Arc`, «Checking the data», gli errori dei punti);
  `intersection.hpp` (`Location`, `Contact`); gli interni della 0.2
  `geom2d/src/curve_meaning.hpp` (`arcRule`), `carriers.hpp` (`ExactPoint`) e
  `exact/quadratic.hpp`

## File

- `geom2d/src/ellipse_meaning.hpp`, `geom2d/src/ellipse_meaning.cpp` (nuovi,
  interni)
- `geom2d/src/ellipse.cpp` (nuovo)
- `tests/geom2d/ellipse_meaning_test.cpp`, `tests/geom2d/ellipse_test.cpp`
  (nuovi)
- `tests/corpus/esegui/ellipse_data.hpp` (nuovo, senza `.cpp`)
- `tests/corpus/esegui/op_ellipse.cpp` (nuovo)
- `tests/corpus/in-attesa/`: cancellare `0.3-RS4a.txt` e `0.3-RS4b.txt`

Nessun `CMakeLists.txt`. `tests/geom2d/ellipse_contract_test.cpp` non si tocca.

## Cosa fare

Realizzare, senza cambiarle, queste dichiarazioni di `ellipse.hpp`:

```cpp
struct Ellipse { Point2 center; Point2 majorAxis; double ratio = 1.0;
                 double startParameter = 0.0; double endParameter = kFullTurn; };
[[nodiscard]] Result<double> magnitude(const Ellipse& ellipse);
[[nodiscard]] Result<Endpoints> endpoints(const Ellipse& ellipse);
[[nodiscard]] Result<bool> meetsRectangle(const Ellipse& ellipse, Point2 corner,
                                          Point2 oppositeCorner);
```

- **Il significato esatto** (`ellipse_meaning.hpp`, interno, che PS7 e PS8
  usano senza cambiarlo): la mappa F con b = rapporto · (−a.y, a.x) esatto;
  l'equazione implicita dell'ellisse portante, a coefficienti esatti; D0 e D1
  da `robust::sinCos`; l'ampiezza nominale calcolata esattamente e la regola di
  `Arc` applicata a lei (riusa `arcRule` della 0.2); la posizione esatta
  (`Location`) di un punto dell'ellisse portante sull'arco, dalla direzione
  F⁻¹(p − centro) a meno di un fattore positivo e dai segni con D0 e D1;
  il controllo dei dati nell'ordine di `curves.hpp` (Decisione, punto 3).
- **L'incontro esatto dell'ellisse con una retta** (sempre in
  `ellipse_meaning.hpp`): la retta sostituita nell'equazione implicita dà un
  polinomio di secondo grado; i punti come `ExactPoint` (a + b·√d), con il
  `Contact` (`crossing`, `touching` per la radice doppia) e la posizione
  sull'arco e sul segmento. PS7 lo usa per `intersect` con le curve diritte,
  così `meetsRectangle` e `intersect` concordano come chiede `ellipse.hpp`.
- **`magnitude` ed `endpoints`** come dice l'intestazione (estremi entro
  `kRelativePointError`, anche oltre `kMaxCoordinate`).
- **`meetsRectangle`**: vero se l'inizio esatto dell'arco sta nel rettangolo
  chiuso, o se l'arco incontra un lato che non è un punto, o se il rettangolo è
  un punto della curva; esatto, senza tolleranza.

Le letture di prova dell'ellisse (`ellipse_data.hpp`, funzioni `inline`, nel
namespace `solid_space::corpus`) le usano anche le operazioni di PS7 e PS8:
leggono la chiave `ellisse` e scrivono una `Location` con `writeName` di
`plane_data.hpp`.

### Casi di verifica (per RS4a e RS4b)

| Dato | Forma in `ingresso` |
|---|---|
| ellisse | un oggetto `{"centro": punto, "asse_maggiore": punto, "rapporto": n, "parametro_iniziale": n, "parametro_finale": n}`, sotto la chiave `ellisse` (o `seconda_ellisse` in PS7) |

| Operazione | `ingresso` | `atteso` |
|---|---|---|
| `ellipseMagnitude` | `ellisse` | `grandezza` (esatta), oppure `diagnosi` (`dato` 0) |
| `ellipseEndpoints` | `ellisse` | `inizio`, `fine` (punti approssimati), oppure `diagnosi` |
| `meetsRectangle` | `ellisse`, `angolo`, `angolo_opposto` | `incontra` (booleano), oppure `diagnosi` (`dato` 0, 1 o 2) |

I nomi `magnitude` ed `endpoints` sono già delle curve (`op_curves.cpp`), e
un nome registrato due volte fa fallire i test: per l'ellisse le operazioni
si chiamano `ellipseMagnitude` ed `ellipseEndpoints` (e `ellipseIntersect` in
PS7). I punti con tolleranza `kRelativePointError · max(M, |p|)`; le
decisioni esatte. L'operazione `meetsRectangle` chiama anche la funzione con gli angoli
scambiati e fa fallire il caso se la risposta cambia.

**RS4a** (fascia forte) copre almeno:

- il significato: parametri 0 e 2π, parametri uguali (ellisse intera),
  parametri che differiscono di un multiplo esatto di `kFullTurn`; iniziale
  maggiore del finale; ampiezze nominali vicine a zero e a un giro, ai due lati
  della regola di `Arc` (scelte, come per gli archi della 0.2, in modo che la
  risposta non cambi per D0 e D1 entro 2 ulp, o con D0 e D1 dallo stesso
  `double`); rapporto 1 e rapporti 0,01 e 1e-6; assi ruotati;
- `meetsRectangle`: rettangolo tangente a un lato dell'ellisse (dentro e
  fuori), ridotto a un segmento tangente e secante, ridotto a un punto
  dell'ellisse e a un punto a un ulp; rettangolo sulla parte mancante di un
  arco; inizio dell'arco su un lato; il caso 6 della #217.

**RS4b** (fascia leggera) copre almeno:

- `magnitude` ed `endpoints` di ellissi intere e di archi, alle scale dell'ADR
  0009 (millimetri vicino all'origine, coordinate di 1e5);
- `meetsRectangle` ordinari: rettangolo che taglia, che contiene, interno
  senza tocco, lontano;
- le diagnosi, nell'ordine dei dati: rapporto fuori da (0, 1], valori non
  finiti, coordinate oltre `kMaxCoordinate`, parametri oltre
  `kMaxTrigArgument`, asse nullo, arco ridotto a una direzione; angoli del
  rettangolo non validi (dato 1 e 2).

Valori attesi: decisioni con `fractions` di Python e numeri della forma
a + b·√d; coordinate con `decimal`.

## Regole della tappa che valgono qui

- **Contratto:** `ellipse.hpp` non cambia, commenti compresi. Se qualcosa manca
  o non torna, fermati come dice AGENTS.md.
- **Gli interni della 0.2** si usano senza cambiarli; ciò che manca va nei tuoi
  file nuovi.
- **Determinismo** (ADR 0008, punto 5): calcolo nei `.cpp`; la sola
  trigonometria è `robust::sinCos` per D0 e D1.
- File comuni: nessuno; `plane_data.*` non si tocca. Cancelli solo i file dei
  tuoi riferimenti in `in-attesa/`.

## Test richiesti

- `ellipse_meaning_test.cpp`: equazione implicita esatta (i punti F(1, 0),
  F(0, 1) e F(−1, 0) la soddisfano); posizione sull'arco di punti noti; incontro
  con rette secanti, tangenti (radice doppia) e a un ulp; ellisse di rapporto 1
  contro le rette di un cerchio con gli stessi punti.
- `ellipse_test.cpp`: `meetsRectangle` coerente con l'incontro con i lati e con
  l'inizio, su dati generati in modo deterministico; angoli scambiati;
  `endpoints` di un'ellisse intera (inizio uguale alla fine).
- I casi di RS4a e RS4b passano.

## Fatto quando

- `magnitude`, `endpoints` e `meetsRectangle` sono realizzate come dichiara
  `ellipse.hpp`; `ellipse_meaning.hpp` documenta il significato e l'incontro con
  le rette per PS7 e PS8.
- I casi di `[0.3-RS4a]` e `[0.3-RS4b]` passano e i loro file sono cancellati da
  `in-attesa/`.
- Il job del determinismo è verde.
- La CI è verde.
