# [0.3-PS7] `geom2d`: intersezioni dell'ellisse

- **Tappa:** 0.3 — [piano](../0.3-regioni-ed-ellisse.md)
- **Issue:** #…
- **Difficoltà:** media
- **Agente:** claude
- **Modello e impegno:** medio; impegno alto. La revisione di Codex è mirata,
  in fascia forte, su tangenze, coniche uguali e posizioni agli estremi
- **Riferimenti:** issue `[0.3-RS7a]` (#…), agente codex, fascia forte;
  `[0.3-RS7b]` (#…), agente codex, fascia leggera; `[0.3-RS7c]` (#…), agente
  codex, fascia leggera (`autocad-ellissi.dxf`)
- **Contratto:** `geom2d/include/solid_space/geom2d/ellipse.hpp` (le due
  `intersect`, `EllipseIntersectionPoint`, `EllipseOverlapEnd`,
  `EllipseOverlap`, `EllipseIntersections`); usa `intersection.hpp`
  (`Extension`, `Location`, `Contact`) e `curves.hpp`
- **Dipende da:** PS3 (#…), PS4 (#…); RS7a (#…), RS7b (#…), RS7c (#…). La
  revisione del contratto `[0.3-CS]` (#222) arriva attraverso PS4
- **Sblocca in Tecnigrafo:** PT50 (issue ancora da aprire), con PS4 e PS8: lo
  snap *Intersezione* sulle ellissi
- **Componente:** `geom2d`
- **Da leggere:** [ADR 0017](../../adr/0017-ellisse-nel-piano.md) (Decisione,
  punti 4, 7, 8 e 9; «Che cosa rende possibili decisioni esatte»); `ellipse.hpp`
  e `intersection.hpp`; le schede [PS3](PS3-polinomi.md) e [PS4](PS4-ellisse.md)
  e le loro intestazioni `exact/polynomial.hpp` ed `ellipse_meaning.hpp`; gli
  interni della 0.2 `carriers.hpp` e `positions.hpp`

## File

- `geom2d/src/ellipse_intersection.cpp` (nuovo); se serve, un'intestazione
  interna nuova tua (per esempio `ellipse_conic.hpp`)
- `tests/geom2d/ellipse_intersection_test.cpp` (nuovo)
- `tests/corpus/esegui/op_ellipse_intersection.cpp` (nuovo), con le letture di
  `ellipse_data.hpp` di PS4
- `tests/corpus/in-attesa/`: cancellare `0.3-RS7a.txt`, `0.3-RS7b.txt` e
  `0.3-RS7c.txt`

Nessun `CMakeLists.txt`. Se il pacchetto supera la misura, l'ellisse con
l'ellisse diventa un pacchetto a sé, con una riga in «Modifiche al piano»
decisa dal responsabile.

## Cosa fare

Realizzare, senza cambiarle, le due `intersect` di `ellipse.hpp`:

```cpp
[[nodiscard]] Result<EllipseIntersections> intersect(const Ellipse& ellipse,
                                                     Extension ellipseExtension, const Curve& curve,
                                                     Extension curveExtension);
[[nodiscard]] Result<EllipseIntersections> intersect(const Ellipse& first, Extension firstExtension,
                                                     const Ellipse& second,
                                                     Extension secondExtension);
```

- **Con una retta** (segmento, pezzo dritto): l'incontro di
  `ellipse_meaning.hpp` (PS4), lo stesso di `meetsRectangle`.
- **Con un cerchio, un arco, un pezzo curvo o un'altra ellisse:** la forma
  razionale della prima ellisse nell'equazione della seconda curva dà un
  polinomio di grado al più 4 (ADR 0017); numero, molteplicità (radice multipla
  = `touching`), posizioni sui due ingressi e ordine con `exact/polynomial.hpp`
  (PS3); il punto della forma razionale che manca (il parametro all'infinito)
  trattato a parte; i punti costruiti restringendo la radice fino a
  `kRelativePointError`.
- **Stesso sostegno:** due ellissi con gli stessi punti, o un'ellisse di
  rapporto 1 e il suo cerchio: sovrapposizioni (`EllipseOverlap`), con
  `whole` e gli estremi, come in `intersection.hpp`.
- Ordine lungo la prima ellisse dal suo inizio in senso antiorario;
  `omittedBeyondDomain`; simmetria delle decisioni fra due ellissi (Decisione,
  punto 7); diagnosi nell'ordine dei dati.
- Un filtro in virgola mobile per le coppie lontane dalle degenerazioni è
  ammesso, con l'errore scritto accanto (Decisione, punto 9).

### Casi di verifica (per RS7a, RS7b e RS7c)

| Operazione | `ingresso` | `atteso` |
|---|---|---|
| `ellipseIntersect` | `ellisse`, `prolunga_ellisse`, e `curva` o `seconda_ellisse`, `prolunga_seconda` | `punti`: elenco di `{"punto": punto, "contatto": nome, "sulla_prima": nome di Location, "sulla_seconda": nome di Location}`; `sovrapposizioni`: elenco di `{"intera": booleano, "da": estremo, "a": estremo}`, con estremo `{"punto": punto, "sulla_prima": nome, "sulla_seconda": nome}`; `omessi_fuori_dominio`: intero; oppure `diagnosi` (`dato` 0 o 1) |

L'ellisse ha la forma di [PS4](PS4-ellisse.md); l'operazione non si chiama
`intersect`, che è già delle curve. Le posizioni sono solo nomi di
`Location` (l'ellisse non ha stazioni). I punti con tolleranza
`kRelativePointError · max(M, |p|)`; il resto esatto. Con due ellissi
l'operazione chiama anche la funzione con gli argomenti scambiati e fa fallire
il caso se le decisioni cambiano (i punti restano entro l'errore).

**RS7a** (fascia forte) copre almeno, alle due scale dell'ADR 0009:

- #217, casi 1a–1c (segmento secante, tangente in (0, 5), a 5 + 1e-12), 2a
  (cerchio tangente in due punti), 2b (due ellissi uguali ruotate di 90°,
  quattro punti), 3a (arco e prolungamento) e 5 (cerchio sotto scala (2, 1) e
  rotazione di 30°, risposta esatta sui `double` dati);
- tangenze esatte e quasi tangenze di un ulp con rette, cerchi ed ellissi;
  tangenze doppie; quattro punti; incontri negli estremi degli archi;
- ellissi con gli stessi punti date in modi diversi (asse opposto, parametri
  spostati di π); ellisse di rapporto 1 sul suo cerchio e su un arco dello
  stesso cerchio; `sameCurve` in un punto solo;
- valori: decisioni con aritmetica esatta e, per il quarto grado, radici
  isolate con successioni di Sturm in razionali; coordinate con `decimal`.

**RS7b** (fascia leggera) copre almeno:

- incroci ordinari con ogni tipo di curva (segmento, arco, cerchio, pezzo dritto
  e curvo) e fra ellissi, con e senza prolungamento;
- `omittedBeyondDomain` (un prolungamento con l'incontro oltre
  `kMaxCoordinate`);
- le diagnosi per l'ellisse (dato 0) e per la curva o la seconda ellisse
  (dato 1).

**RS7c** (fascia leggera), dopo che il responsabile ha pubblicato
`autocad-ellissi.dxf` (piano, sezione 5), e dopo RS5c (`sorgenti.json`):
aggiunge la voce del file e la sua copia in `esterni/`; con le ellissi e gli
archi d'ellisse del file (fatti con `ELLIPSE` in tutti i modi, ruotati,
specchiati, esplosi da un blocco con scala (2, 1)) e con le altre entità del
file scrive le intersezioni, ricavate con l'aritmetica esatta sui dati del
file; un arco con parametri uguali, se c'è, conferma la regola dell'ellisse
intera. Se il file la contraddice, si ferma e apre una issue `contratto`.

## Regole della tappa che valgono qui

- **Contratto:** `ellipse.hpp` e `intersection.hpp` non cambiano, commenti
  compresi. Se qualcosa manca o non torna, fermati come dice AGENTS.md.
- **Gli interni** della 0.2, di PS3 e di PS4 si usano senza cambiarli; ciò che
  manca va nei tuoi file nuovi.
- **Determinismo** (ADR 0008, punto 5): calcolo nei `.cpp`; la sola
  trigonometria è `robust::sinCos`, già in PS4.
- File comuni: nessuno; cancelli solo i file dei tuoi riferimenti.

## Test richiesti

- `ellipse_intersection_test.cpp`, su dati generati in modo deterministico: lo
  scambio di due ellissi dà le stesse decisioni; ogni punto trovato sta
  esattamente su entrambi i sostegni (sostituzione esatta); con il segmento,
  stessi punti di `meetsRectangle` sui lati; un'ellisse di rapporto 1 e il suo
  cerchio danno una sovrapposizione `whole`.
- I casi di RS7a, RS7b e RS7c passano.

## Fatto quando

- Le due `intersect` sono realizzate come dichiara `ellipse.hpp`.
- I casi di `[0.3-RS7a]`, `[0.3-RS7b]` e `[0.3-RS7c]` passano e i loro file sono
  cancellati da `in-attesa/`.
- Il job del determinismo è verde.
- La CI è verde.
