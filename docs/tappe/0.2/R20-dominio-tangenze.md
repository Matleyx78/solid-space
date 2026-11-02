# [0.2-R20] Riferimenti: dominio delle tangenze

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** #146
- **Agente:** codex
- **Difficoltà:** impegnativa
- **Modello e impegno:** forte; impegno alto
- **Riferimenti del pacchetto:** [P20](P20-dominio-tangenze.md), #145
- **Contratto:** `geom2d/include/solid_space/geom2d/curves.hpp`, `tangency.hpp`
- **Dipende da:** P1 (#34), C (#133, con la pull request delle decisioni unita)
- **Da leggere:** `curves.hpp` (regole generali e dominio) e `tangency.hpp`;
  [ADR 0013](../../adr/0013-esatto-dove-serve.md), punti 4 e 5; le decisioni
  #149–#152, #155 e #157; schede [P15](P15-tangenti.md) e [P16](P16-raccordi.md);
  [README dell'archivio](../../../tests/corpus/README.md) e AGENTS.md

Questa scheda sostituisce il testo della issue #146. I casi nei subnormali non
servono più: l'ADR 0013 (2026-11-02) toglie i subnormali dal contratto delle
costruzioni. Restano i casi al confine di `kMaxCoordinate` (K = 2^50) e l'ordine
dei controlli delle tangenze. I casi al confine dell'offset sono in R19 (#129),
non qui.

## Cosa verificare

- **Centri oltre K (#149):**
  `tangentCircles(Segment{(K−1/8, −1), (K−1/8, 1)}, Segment{(0,0), (1,0)}, 1/4)`:
  `outOfDomain`, dato 2 (due centri esatti a x = K+1/8, che arrotondati
  tornerebbero a K). Lo stesso per `tangentCircleNear` e per `fillet` (regola 3).
- **Punti di tangenza oltre K con il centro entro K (#149):** un cerchio
  tangente il cui centro esatto è entro K e un punto di tangenza esatto oltre K
  dà `outOfDomain`, dato 2, per `tangentCircles`, `tangentCircleNear` e
  `fillet`. Per `fillet`, anche il piede della perpendicolare della regola 1
  oltre K.
- **`tangentsFrom` (#151):** con `Extension::extended`, un punto di tangenza
  esatto oltre K dà `outOfDomain`, dato 1; con `Extension::none` lo stesso
  punto, se non sta sulla curva data, non si elenca e non fa fallire la
  chiamata.
- **Ordine dei controlli (#150):** un clic non valido e una costruzione oltre K
  insieme danno la diagnosi del clic (`tangentCircleNear`, dato 3 o 4); in
  `fillet`, regola 3, un clic sulla retta o sul cerchio dell'altra curva e un
  centro oltre K danno `ambiguousPick`.
- **Valori entro K vicino al confine:** per esempio
  `tangentsFrom((K−1, 2), Circle{(K−1, 0), 1}, Extension::extended)`, con
  punti (K−1 ± √3/2, 1/2), entro la tolleranza del punto. Che la coordinata
  restituita non superi K il formato non lo sa esprimere: lo prova P20 con un
  test di unità.

Non stanno nel corpus, perché il loro esito è in una fascia (ADR 0013, punti 4
e 5): il bulge 0 con punti di tangenza restituiti coincidenti (#152) e m
ricostruito a meno dell'errore dell'arco da K (#155, #157). Nessun caso li
impone.

## File

Solo `tests/corpus/`: casi nuovi in `casi/geom2d/` e `in-attesa/0.2-R20.txt`,
una riga per caso con `#145`.

## Valori attesi e fatto quando

Da aritmetica esatta, mai eseguendo solid-space; punti e stazioni con la
tolleranza del loro errore dichiarato, decisioni ed esiti esatti. Nel
`procedimento`, il calcolo che porta il valore esatto oltre K o entro K. Se il
contratto non determina un caso, fermarsi secondo AGENTS.md. I casi passano la
validazione e sono pubblicati separatamente dal lavoro P20, con CI verde.
