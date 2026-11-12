# [0.3-PS6] `geom2d`: arrangiamento delle curve (interno)

- **Tappa:** 0.3 — [piano](../0.3-regioni-ed-ellisse.md)
- **Issue:** #…
- **Difficoltà:** impegnativa
- **Agente:** claude
- **Modello e impegno:** forte; impegno alto. Resta impegnativo: la topologia
  (tratti comuni a più curve, facce con il bordo che tocca se stesso, gruppi
  connessi) e le prestazioni senza tutte le coppie sono casi limite che nessun
  documento fissa al livello interno, e nessun riferimento lo copre prima di PS9
- **Riferimenti:** non servono: nessuna funzione pubblica; lo provano i test di
  unità, e attraverso `detectBoundary` i casi di RS9a–RS9d. La pull request la
  rivede Codex, in fascia forte
- **Contratto:** usa `geom2d/include/solid_space/geom2d/boundary.hpp` (passi 5
  e 6 di `detectBoundary`: a chi va un tratto comune, come si percorre il bordo
  di una faccia) e `curves.hpp`; non realizza funzioni pubbliche
- **Dipende da:** la revisione del contratto `[0.3-CS]` (#222); PS2 (#…)
- **Sblocca in Tecnigrafo:** niente da solo; serve a PS9 (PT43, PT44)
- **Componente:** `geom2d` (interno)
- **Da leggere:** [ADR 0014](../../adr/0014-contorno-e-isole-di-una-regione.md),
  con le righe **Corretto:** («Che cosa rende la risposta coerente», Decisione
  punti 4, 6 e 9, i rischi in «Conseguenze»); `boundary.hpp` (i passi 5–7);
  la scheda [PS2](PS2-intorno-e-parita.md) e le sue intestazioni; gli interni
  della 0.2 `carriers.hpp` (`meet`), `positions.hpp` (`locate`,
  `compareAlong`) e `curve_meaning.hpp`; la scheda [PS9](PS9-contorno.md), che
  usa il tuo `arrangement.hpp`

## File

- `geom2d/src/arrangement.hpp`, `geom2d/src/arrangement.cpp` (nuovi)
- `tests/geom2d/arrangement_test.cpp` (nuovo)

Nessun `CMakeLists.txt`. Se il pacchetto supera la misura (circa 600 righe di
codice), si divide fra la divisione delle curve (passo 5, primo capoverso) e
le facce, con una riga in «Modifiche al piano» decisa dal responsabile.

## Perché serve

`detectBoundary` è definito dall'arrangiamento delle curve (ADR 0014): le curve
divise in tutti i punti in cui si incontrano, i tratti comuni ridotti a un pezzo
solo, le facce del piano tracciate con l'ordine esatto dei pezzi in ogni
vertice. È la parte delicata dell'ADR, insieme alle prestazioni con migliaia di
curve. Sta in un pacchetto a sé, interno, perché separa queste decisioni dai
passi di `detectBoundary` già scritti nell'intestazione (PS9, fascia media).

## Cosa fare

Un'intestazione interna, `arrangement.hpp`, nel namespace
`solid_space::geom2d::detail`. Nomi e firme li fissi tu; l'intestazione li
documenta, perché li usa PS9. Tutto esatto, sui `double` dati.

- **Ingresso:** un elenco di curve già controllate (segmenti, cerchi, pezzi di
  polilinea dritti e curvi; gli archi li converte PS9 al passo 1), ciascuna con
  un indice e un ordine di precedenza (le curve prima dei collegamenti, poi
  l'indice più basso, come dice il passo 5); le curve possono essere
  prolungate fino a un punto esatto (passo 3).
- **Divisione:** ogni curva divisa in tutti i punti in cui ne incontra
  un'altra o finisce (`meet` e posizioni della 0.2); un tratto comune a più
  curve è un solo pezzo, attribuito alla curva di precedenza più alta; ogni
  pezzo con la sua origine (curva e verso).
- **Vertici e facce:** i pezzi ordinati intorno a ogni vertice con l'ordine di
  PS2; le facce del piano, con la faccia illimitata; il bordo di una faccia
  percorso con la faccia a sinistra, saltando i pezzi che hanno la faccia dai
  due lati, e diviso in loop semplici in ogni vertice in cui passa più volte
  (passo 6).
- **Faccia di un punto:** la faccia che contiene un punto esatto che non sta su
  nessun pezzo (con la parità o l'ordine di PS2).
- **Gruppi connessi:** quali curve e collegamenti si toccano; il bordo della
  faccia illimitata di un gruppo preso da solo; per un loop, quali altri loop
  lo racchiudono (per la profondità del passo 7).
- **Prestazioni** (ADR 0014, punto 9): non si provano tutte le coppie; ingombri
  per eccesso, una griglia o il solo gruppo intorno al punto, purché il
  risultato sia lo stesso. Un filtro in virgola mobile davanti alle decisioni
  esatte è ammesso, con l'errore scritto accanto.

## Regole della tappa che valgono qui

- **Contratto:** il pacchetto non tocca le intestazioni pubbliche. Le regole
  dei passi 5 e 6 di `boundary.hpp` (a chi va un tratto comune, i pezzi con la
  faccia dai due lati, la divisione in loop semplici) le segui come sono; se
  una non determina un caso, fermati come dice AGENTS.md («Un buco nel
  contratto»).
- **Gli interni** della 0.2 e di PS2 si usano senza cambiarli.
- **Determinismo** (ADR 0008, punto 5): calcolo nei `.cpp`, niente funzioni
  trigonometriche di `<cmath>`; nessun ordine che dipenda da indirizzi di
  memoria o da contenitori non ordinati.
- Nessuna eccezione per errori attesi; solo `std::bad_alloc`.
- I file di `tests/corpus/in-attesa/` non ti riguardano.

## Test richiesti

- `arrangement_test.cpp`: un rettangolo di quattro segmenti con un cerchio
  dentro (due facce limitate, un gruppo per ciascuno); linee che sporgono oltre
  un angolo (i pezzi sporgenti hanno la stessa faccia dai due lati); vertici
  con quattro o più pezzi; cerchi tangenti dentro e fuori nello stesso punto;
  tratti comuni a tre curve (attribuiti alla curva di indice più basso, a una
  curva prima di un collegamento); una polilinea che si interseca; una faccia
  il cui bordo tocca se stesso in un vertice (due loop semplici); un cerchio
  con un diametro; gruppi dentro un'isola.
- Proprietà su dati generati in modo deterministico: permutando le curve le
  facce restano le stesse e le origini si permutano; la faccia di un punto non
  cambia aggiungendo curve lontane che non toccano le altre.
- Un caso con qualche migliaio di segmenti di una griglia, che finisce in un
  tempo ragionevole anche in Debug (la misura vera è del banco, PS11).

## Fatto quando

- `arrangement.hpp` documenta che cosa offre a PS9, e i test lo provano.
- I casi delle tappe precedenti passano senza cambiamenti.
- La CI è verde.
