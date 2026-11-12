# [0.3-PS10] `geom2d`: contorni scelti uno dentro l'altro

- **Tappa:** 0.3 — [piano](../0.3-regioni-ed-ellisse.md)
- **Issue:** #260
- **Difficoltà:** media
- **Agente:** codex
- **Modello e impegno:** medio; impegno alto. La revisione di Claude è mirata,
  in fascia forte, sui contorni che si toccano e sui conflitti
- **Riferimenti:** issue `[0.3-RS10a]` (#258), agente claude, fascia forte;
  `[0.3-RS10b]` (#259), agente claude, fascia leggera
- **Contratto:** `geom2d/include/solid_space/geom2d/boundary.hpp`
  (`nestContours`, `ContourConflictKind`, `ContourConflict`, `ContourNesting`);
  usa `region.hpp` (`Loop`, `SkippedInput`)
- **Dipende da:** PS5 (#245); RS10a (#258), RS10b (#259). La revisione del contratto
  `[0.3-CS]` (#222) e PS2 arrivano attraverso PS5
- **Sblocca in Tecnigrafo:** PT43 ([Matleyx78/tecnigrafo#740](https://github.com/Matleyx78/tecnigrafo/issues/740)), con PS9: le isole
  dei contorni scelti (*Select objects*); con PS5 e PS9, PT44 (`-HATCH`)
- **Componente:** `geom2d`
- **Da leggere:** [ADR 0014](../../adr/0014-contorno-e-isole-di-una-regione.md)
  (Decisione, punto 7; casi 6a–6c); il commento di `nestContours` in
  `boundary.hpp` e `region.hpp`; la scheda [PS5](PS5-taglio.md) e la sua
  intestazione interna `region_parts.hpp` (controllo dei loop, parti esatte di
  una curva rispetto a una regione), su cui costruisci; la scheda
  [PS2](PS2-intorno-e-parita.md)

## File

- `geom2d/src/nest_contours.cpp` (nuovo)
- `tests/geom2d/nest_contours_test.cpp` (nuovo)
- `tests/corpus/esegui/op_nest_contours.cpp` (nuovo)
- `tests/corpus/in-attesa/`: cancellare `0.3-RS10a.txt` e `0.3-RS10b.txt`

Nessun `CMakeLists.txt`. `detectBoundary`, nella stessa intestazione, è di PS9,
in un file suo: questo pacchetto non la tocca.

## Cosa fare

Realizzare, senza cambiarla, `nestContours` di `boundary.hpp`:

```cpp
[[nodiscard]] ContourNesting nestContours(std::span<const Loop> contours);
```

- **Il controllo** di ogni contorno è quello di `region_parts.hpp` (PS5): gli
  scartati in `skipped`, con `nullopt` in `depth`.
- **La relazione di due contorni** A e B si legge dalle classi esatte delle
  parti di B rispetto alla regione del solo A (`region_parts.hpp`): A racchiude
  B quando qualche parte di B è dentro e nessuna è fuori; sono `same` quando
  ogni parte di ciascuno sta sul bordo dell'altro; altrimenti `crossing` se B ha
  parti dentro e fuori o se si racchiudono a vicenda; nessuna relazione se B è
  tutto fuori o sul bordo. Esatta, senza tolleranza e senza costruzioni.
- **La profondità** di un contorno è il numero dei contorni che lo racchiudono;
  i conflitti, elencati una volta per coppia (first < second, in ordine), non
  contano.
- Un indice per non confrontare le coppie lontane (ingombri) è ammesso, purché
  il risultato non cambi.

### Casi di verifica (per RS10a e RS10b)

| Operazione | `ingresso` | `atteso` |
|---|---|---|
| `nestContours` | `contorni` (elenco di loop: `{"cerchio": …}` o una polilinea `{"vertici": …, "chiusa": true}`) | `profondita`: elenco, uno per contorno, di interi o di `false` per un contorno scartato; `conflitti`: elenco di `{"primo": intero, "secondo": intero, "tipo": "crossing" o "same"}`; `scartati`: elenco di `{"indice": intero, "diagnosi": {...}}` |

Tutto esatto. L'operazione chiama anche la funzione con i contorni in ordine
inverso e fa fallire il caso se le profondità e i conflitti, riportati agli
indici di partenza, cambiano.

**RS10a** (fascia forte) copre almeno, alle due scale dell'ADR 0009:

- #214, caso 6b: due contorni che si toccano in un punto, da fuori (0 e 0) e da
  dentro (0 e 1); un contorno che corre lungo un lato dell'altro, da dentro e
  da fuori;
- contorni uguali dati in modi diversi (verso opposto, altro vertice
  iniziale, un cerchio e la polilinea di due semicerchi): `same`;
- polilinee che si intersecano (regola pari-dispari) e contorni dentro le loro
  parti; contorni che si toccano in un vertice e in una tangenza fra archi.

**RS10b** (fascia leggera) copre almeno:

- #214, casi 6a (profondità 0, 1, 1, 2) e 6c (`crossing`);
- contorni lontani; un contorno dentro una catena di quattro; contorni non
  validi (ogni diagnosi, il cerchio di raggio zero, la polilinea ridotta a un
  punto) in mezzo ai buoni.

Valori attesi: decisioni con `fractions` di Python e numeri della forma
a + b·√d.

## Regole della tappa che valgono qui

- **Contratto:** `boundary.hpp` e `region.hpp` non cambiano, commenti compresi.
  Se qualcosa manca o non torna, fermati come dice AGENTS.md.
- **Gli interni** della 0.2, di PS2 e di PS5 si usano senza cambiarli; se a
  `region_parts.hpp` manca qualcosa, fermati e scrivilo nella issue.
- **Determinismo** (ADR 0008, punto 5): calcolo nei `.cpp`.
- File comuni: nessuno; cancelli solo i file dei tuoi riferimenti.

## Test richiesti

- `nest_contours_test.cpp`, su dati generati in modo deterministico: permutare
  i contorni permuta le profondità e i conflitti; invertire il verso o ruotare
  il vertice iniziale di una polilinea non cambia nulla; ogni contorno di
  profondità k ha esattamente k contorni che lo racchiudono.
- I casi di RS10a e RS10b passano.

## Fatto quando

- `nestContours` è realizzata come dichiara `boundary.hpp`.
- I casi di `[0.3-RS10a]` e `[0.3-RS10b]` passano e i loro file sono cancellati
  da `in-attesa/`.
- Il job del determinismo è verde.
- La CI è verde.
