# [0.2-R19] Riferimenti: bulge e vertici della curva parallela

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** #129
- **Agente:** codex
- **Difficoltà:** impegnativa
- **Modello e impegno:** forte; impegno alto
- **Riferimenti del pacchetto:** [P19](P19-bulge-parallela.md), #130
- **Contratto:** `geom2d/include/solid_space/geom2d/offset.hpp`, regole generali
  di `curves.hpp`
- **Dipende da:** P1 (#34), C (#133, con le decisioni unite)
- **Da leggere:** [ADR 0012](../../adr/0012-curva-parallela-nel-piano.md), punti
  3, 5–7 e 9; decisioni #127 e #132, [CONTRATTI.md](../../CONTRATTI.md);
  [README dell'archivio](../../../tests/corpus/README.md) e AGENTS.md

Questa scheda riprende l'ambito della #129 e lo allarga secondo la decisione
#132. Non realizza P19 e non modifica il codice.

## Cosa verificare

- Il caso della #127, sia `offsetCurve` sia `offsetPolyline`: arco quasi intero
  a x = 1e6, corda di un ulp, bulge 2^32, d = −0,03125; `done`, raggio circa
  0,15625 e `simple` vero.
- Archi quasi interi con corde di pochi ulp: almeno un `done` e il caso
  `failed` / `notRepresentable` della #132. Ora i vertici sono determinati:
  ogni coordinata è il double più vicino al valore esatto, pareggio al pari;
  il bulge e il controllo di rappresentabilità usano quei double.
- Tutti i vertici: estremi di pezzi dritti e curvi, capi liberi, tagli,
  prolungamenti, punti di giunzione e estremi degli archi di giunzione;
  includere pareggi esatti tra double, risolti al pari, anche nei percorsi
  del codice P13/P14 già unito.
- Un arco non nullo i cui estremi arrotondati coincidono: `failed` per
  `offsetCurve`, `failed` con `notRepresentable` per `offsetPolyline`; non
  diventa un pezzo puntiforme da eliminare.
- Bulge ricostruito su pezzi tagliati e archi di giunzione; semicirconferenze
  esatte con bulge ±1.
- Correggere i casi R13/R14 che impongono un bulge esatto non più garantito:
  ricalcolo indipendente o tolleranza ricavata dall'errore della curva, come
  dice P14. Il pavimento generale della scala è 2^-1022; R20 tratta i casi
  subnormali in modo sistematico.

## File

Solo `tests/corpus/`: casi nuovi in `casi/geom2d/`, correzioni dei casi R13/R14
necessarie per la nuova regola e `in-attesa/0.2-R19.txt` (casi nuovi con #130).

## Valori attesi e fatto quando

Calcoli a mano o aritmetica esatta e ad alta precisione, mai eseguendo
solid-space. Documentare il calcolo dell'arrotondamento, del bulge e delle
risposte discrete; se il contratto non determina un caso, fermarsi secondo
AGENTS.md. I casi coprono le voci sopra, passano la validazione e sono
pubblicati separatamente dal lavoro P19, con CI verde.
