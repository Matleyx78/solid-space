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

Questa scheda riprende l'ambito della #129 e lo allarga secondo le decisioni
#132, #148, #149, #153 e #154. Non realizza P19 e non modifica il codice.

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
- I tre casi #148: segmento (0,0)–(2^-60,2^-60), d = 1 (`failed`);
  polilinea di un pezzo (`failed` / `notRepresentable`); lo stesso pezzo fra
  due pezzi lunghi (scarto e `simple` falso, con `origins` allineate).
- Dominio sui valori esatti (#149), K = 2^50: segmento x = K−1/8 con
  d = −1/4 (`outOfDomain`, dato 1), stessa polilinea (`beyondDomain`),
  cerchio r = K con d = −1/8 (`outOfDomain`, dato 1); nessun esatto entro K
  restituito oltre K, punto m ricostruito esattamente dai double ricevuti.
- **#153**, K = 2^50, t = 3·2^-34: segmento (K, 0)–(K−1/8, t),
  d = −2^26: `offsetCurve` dà `outOfDomain`, dato 1, prima del collasso;
  polilinea aperta degli stessi due vertici: `failed` / `notRepresentable`.
- **#154**, stessi K, t e d: polilinea aperta (K, 0), (K−1/8, t),
  (K−1/8−2^17, t·(1+2^20)), bulge 0. Il primo pezzo si scarta:
  `done`, `simple` falso, un solo pezzo che parte da (K, 2^26).
  L'estremo esatto (K+0,09375, …) solo del pezzo scartato non conta.
- Bulge ricostruito su pezzi tagliati e archi di giunzione; semicirconferenze
  esatte con bulge ±1.
- Correggere i casi R13/R14 che impongono un bulge esatto non più garantito:
  ricalcolo indipendente o tolleranza ricavata dall'errore della curva, come
  dice P14. Il pavimento generale della scala è 2^-1022; R20 tratta i casi
  subnormali in modo sistematico.

La #154 precisa che il dominio esatto si controlla dopo lo scarto dei
dritti collassati, solo sugli estremi dei pezzi rimasti. A un vertice
condiviso contano entrambi i precursori: fine esatta del precedente e inizio
esatto del successivo. Estremi appartenenti solo a pezzi scartati non contano;
m resta ricostruito dai double restituiti. P19 adegua il codice già unito
se necessario, R19 aggiunge l'esempio della #154.

## File

Solo `tests/corpus/`: casi nuovi in `casi/geom2d/`, correzioni dei casi R13/R14
necessarie per la nuova regola e `in-attesa/0.2-R19.txt` (casi nuovi con #130).

## Valori attesi e fatto quando

Calcoli a mano o aritmetica esatta e ad alta precisione, mai eseguendo
solid-space. Documentare il calcolo dell'arrotondamento, del bulge e delle
risposte discrete; se il contratto non determina un caso, fermarsi secondo
AGENTS.md. I casi coprono le voci sopra, passano la validazione e sono
pubblicati separatamente dal lavoro P19, con CI verde.
