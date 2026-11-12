# [0.3-PS5] `geom2d`: parti di una curva dentro una regione

- **Tappa:** 0.3 — [piano](../0.3-regioni-ed-ellisse.md)
- **Issue:** #…
- **Difficoltà:** media
- **Agente:** codex
- **Modello e impegno:** medio; impegno alto. La revisione di Claude è mirata,
  in fascia forte, sulle classi ai vertici, alle tangenze e lungo i lati
- **Riferimenti:** issue `[0.3-RS5a]` (#…), agente claude, fascia forte;
  `[0.3-RS5b]` (#…), agente claude, fascia leggera; `[0.3-RS5c]` (#…), agente
  claude, fascia leggera (`autocad-retini.dxf`)
- **Contratto:** `geom2d/include/solid_space/geom2d/clipping.hpp`
  (`clipToRegion`); usa `region.hpp` (`Loop`, `SkippedInput`),
  `intersection.hpp`, `side.hpp`, `curves.hpp`
- **Dipende da:** la revisione del contratto `[0.3-CS]` (#222); PS2 (#…);
  RS5a (#…), RS5b (#…), RS5c (#…)
- **Sblocca in Tecnigrafo:** PT42 (issue ancora da aprire), righe dei motivi
  tagliate sulla regione; con PS9 e PS10, PT44 (`-HATCH`)
- **Componente:** `geom2d`
- **Da leggere:** [ADR 0015](../../adr/0015-parti-di-una-curva-dentro-una-regione.md),
  con la riga **Corretto:** (#229); `clipping.hpp`, `region.hpp` e il commento
  di `nestContours` in `boundary.hpp` (il controllo dei loop); la scheda
  [PS2](PS2-intorno-e-parita.md) e le sue intestazioni `local_order.hpp` e
  `winding.hpp`; gli interni della 0.2 `carriers.hpp`, `positions.hpp` e
  `curve_meaning.hpp`; `intersect_along.cpp` della 0.2, come esempio di tagli
  esatti lungo un ospite; la scheda [PS10](PS10-contorni-scelti.md), che usa
  il tuo `region_parts.hpp`

## File

- `geom2d/src/region_parts.hpp`, `geom2d/src/region_parts.cpp` (nuovi,
  interni)
- `geom2d/src/clipping.cpp` (nuovo)
- `tests/geom2d/region_parts_test.cpp`, `tests/geom2d/clipping_test.cpp`
  (nuovi)
- `tests/corpus/esegui/op_clipping.cpp` (nuovo)
- `tests/corpus/in-attesa/`: cancellare `0.3-RS5a.txt`, `0.3-RS5b.txt` e
  `0.3-RS5c.txt`

Nessun `CMakeLists.txt`.

## Cosa fare

Realizzare, senza cambiarla, `clipToRegion` di `clipping.hpp`:

```cpp
[[nodiscard]] RegionClipping clipToRegion(std::span<const Curve> hosts, Extension hostExtension,
                                          std::span<const Loop> region);
```

Con i tre passi esatti dell'intestazione (tagli, classi, tratti), gli errori
degli estremi (`StretchEnd`), le due liste indipendenti per ospite (#229),
l'indipendenza di ogni ospite (stessi bit da solo e con gli altri), gli scarti
e nessuna diagnosi per la chiamata.

**`region_parts.hpp`** (interno, nel namespace `solid_space::geom2d::detail`),
che PS10 usa senza cambiarlo:

- il **controllo di un loop** come dice `nestContours` (`invalidValue`,
  `outOfDomain` anche per un punto di un pezzo o del cerchio di un pezzo
  curvo, `degenerateCurve` per un loop che è esattamente un punto), con i pezzi
  ridotti a un punto tolti;
- le **parti esatte di una curva** rispetto a una regione di loop già
  controllati: i tagli esatti (passo 1), ogni parte con la sua classe (dentro,
  fuori, bordo con il lato) dal passo 2, con l'ordine intorno ai tagli e la
  parità di PS2.

`clipToRegion` costruisce i tratti (passo 3) e gli estremi arrotondati sopra
queste parti. Un indice dei pezzi dei loop (ingombri, ordinamento lungo la
normale delle righe parallele) è ammesso per le prestazioni, purché il
risultato di ogni ospite non cambi (ADR 0015, punto 9).

### Casi di verifica (per RS5a, RS5b e RS5c)

| Operazione | `ingresso` | `atteso` |
|---|---|---|
| `clipToRegion` | `ospiti` (elenco di curve), `prolunga_ospiti`, `regione` (elenco di loop: `{"cerchio": …}` o una polilinea `{"vertici": …, "chiusa": true}`) | `ospiti`: elenco, uno per ospite, di `{"dentro": [tratto…], "bordo": [tratto con "lato"…]}`, con tratto `{"da": estremo, "a": estremo}` ed estremo `{"punto": punto, "sull_ospite": posizione}`, e `"lato"`: `left` o `right`; `ospiti_scartati`, `loop_scartati`: elenchi di `{"indice": intero, "diagnosi": {...}}` |

Posizione e punto come in `tests/corpus/README.md`, «Dati del piano»; i punti
con tolleranza `kRelativePointError · max(M, |p|)`, le stazioni con
`kRelativeStationError · max(M, |s|)`, con la M dell'intestazione. Numero,
ordine, classe, lato e `Location` degli estremi sono esatti. L'operazione
chiama anche ogni ospite da solo e fa fallire il caso se i bit cambiano.

**RS5a** (fascia forte) copre almeno, alle due scale dell'ADR 0009:

- i vertici: riga per un vertice convesso e concavo, da dentro e da fuori
  (#215, casi 2a–2c); riga per un vertice in cui si toccano due loop; riga per
  il punto in cui un loop si interseca;
- le tangenze: a un arco del contorno e di un'isola, da dentro e da fuori
  (casi 2d e 2e); a due archi tangenti fra loro nello stesso punto;
- i lati: riga lungo un lato del contorno e di un'isola, con la regione dai due
  lati possibili (caso 3); lungo un tratto comune a due loop che si annullano;
  ospite ad arco sullo stesso cerchio di un pezzo curvo;
- ospiti chiusi (cerchi, archi di un giro intero) interi dentro, sul bordo, con
  l'inizio dentro la regione; un ospite preso com'è che comincia dentro;
- loop che si attraversano, polilinee che si intersecano, loop uguali.

**RS5b** (fascia leggera) copre almeno:

- #215, caso 1: rettangolo con un cerchio come isola e righe a 0°, 45° e 90°,
  intere (`extended`) e prese come sono, alle due scale; una riga che non
  incontra il cerchio (un solo tratto); caso 4, isole annidate; caso 5,
  contorno con archi;
- regione vuota; loop non validi e ospiti non validi in mezzo a quelli buoni,
  con l'indice e la diagnosi;
- un `Segment` con gli estremi scambiati: tratti in ordine inverso, lati
  scambiati.

**RS5c** (fascia leggera), dopo che il responsabile ha pubblicato
`autocad-retini.dxf` (piano, sezione 5): aggiunge la voce del file in
`sorgenti.json` e la sua copia in `esterni/`; dai contorni dei retini salvati
da AutoCAD (`ANSI31`, `NET`, isole annidate, motivo ruotato di 30° e scalato 2)
prende i loop, e dalle definizioni dei motivi alcune righe intere; le risposte
le ricava con l'aritmetica esatta sui dati del file (`provenienza`
`aritmetica-esatta`, che cita la voce).

Valori attesi: decisioni con `fractions` di Python e numeri della forma
a + b·√d; punti e stazioni con `decimal`, confrontati con la tolleranza
dell'errore garantito (ADR 0013, punto 5).

## Regole della tappa che valgono qui

- **Contratto:** `clipping.hpp`, `region.hpp` e `boundary.hpp` non cambiano,
  commenti compresi. Se qualcosa manca o non torna, fermati come dice
  AGENTS.md.
- **Gli interni** della 0.2 e di PS2 si usano senza cambiarli.
- **Determinismo** (ADR 0008, punto 5): calcolo nei `.cpp`, niente funzioni
  trigonometriche di `<cmath>`.
- File comuni: nessuno; cancelli solo i file dei tuoi riferimenti in
  `in-attesa/`.

## Test richiesti

- `region_parts_test.cpp`: controllo dei loop (ogni diagnosi, il loop ridotto
  a un punto); classi delle parti su dati con vertici, tangenze e lati comuni.
- `clipping_test.cpp`, su dati generati in modo deterministico: permutando i
  loop, invertendone il verso o ruotando il vertice iniziale le decisioni non
  cambiano; ogni ospite dà gli stessi bit da solo e con gli altri; tratti
  dentro e sul bordo non si sovrappongono; per un solo loop convesso e una
  riga intera, al più un tratto dentro.
- I casi di RS5a, RS5b e RS5c passano.

## Fatto quando

- `clipToRegion` è realizzata come dichiara `clipping.hpp`; `region_parts.hpp`
  documenta che cosa offre a PS10.
- I casi di `[0.3-RS5a]`, `[0.3-RS5b]` e `[0.3-RS5c]` passano e i loro file sono
  cancellati da `in-attesa/`.
- Il job del determinismo è verde.
- La CI è verde.
