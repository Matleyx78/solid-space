# [0.3-PS2] `geom2d`: ordine intorno a un punto e parità (interno)

- **Tappa:** 0.3 — [piano](../0.3-regioni-ed-ellisse.md)
- **Issue:** #…
- **Difficoltà:** impegnativa
- **Agente:** claude
- **Modello e impegno:** forte; impegno alto. Resta impegnativo: l'ordine
  esatto con tangenze e curvature uguali, in punti con radici, è il punto
  delicato degli ADR 0014 e 0015, e nessun riferimento lo copre prima dei
  pacchetti che lo usano
- **Riferimenti:** non servono: nessuna funzione pubblica; lo provano i test di
  unità, che controllano ogni risposta con l'aritmetica esatta, e attraverso le
  funzioni pubbliche i casi di RS5a–RS5c, RS9a–RS9d, RS10a e RS10b. La pull
  request la rivede Codex, in fascia forte
- **Contratto:** usa `geom2d/include/solid_space/geom2d/region.hpp` (`Loop`) e
  `curves.hpp`; non realizza funzioni pubbliche
- **Dipende da:** la revisione del contratto `[0.3-CS]` (#222)
- **Sblocca in Tecnigrafo:** niente da solo; serve a PS5 (PT42), PS6 e PS9
  (PT43), PS10 (PT43)
- **Componente:** `geom2d` (interno)
- **Da leggere:** [ADR 0014](../../adr/0014-contorno-e-isole-di-una-regione.md)
  («Che cosa rende la risposta coerente», punto 1) e
  [ADR 0015](../../adr/0015-parti-di-una-curva-dentro-una-regione.md) («Che
  cosa rende la risposta coerente» e il rischio in «Conseguenze»);
  `region.hpp`, `clipping.hpp` (passi 1 e 2) e `boundary.hpp` (passi 5 e 6),
  per sapere a che cosa servono le risposte; gli interni della 0.2:
  `geom2d/src/carriers.hpp` (`ExactPoint`, `Carrier`, `meet`),
  `geom2d/src/positions.hpp`, `geom2d/src/curve_meaning.hpp` e
  `geom2d/src/exact/`

## File

- `geom2d/src/local_order.hpp`, `geom2d/src/local_order.cpp` (nuovi)
- `geom2d/src/winding.hpp`, `geom2d/src/winding.cpp` (nuovi)
- `tests/geom2d/local_order_test.cpp`, `tests/geom2d/winding_test.cpp` (nuovi)

Nessun `CMakeLists.txt`.

## Perché serve

Il taglio (`clipToRegion`, PS5) classifica le parti di una riga guardando la
regione ai due lati di ogni taglio; l'arrangiamento (PS6) traccia le facce
girando intorno a ogni vertice; i contorni scelti (PS10) e la profondità dei
loop chiedono se un punto è dentro, fuori o sopra dei loop. Sono la stessa
domanda locale: in che ordine escono i pezzi da un punto, e quale settore fra
due pezzi consecutivi è dentro la regione. Un errore qui darebbe facce
sbagliate o tratti spezzati proprio nei casi che le richieste #214 e #215
citano (vertici, tangenze, righe lungo un lato). Per questo sta in un pacchetto
a sé, prima di PS5 e PS6, che lo usano senza cambiarlo.

## Cosa fare

Due intestazioni interne, nel namespace `solid_space::geom2d::detail`. Nomi e
firme li fissi tu; le intestazioni li documentano, perché li usano PS5, PS6,
PS9 e PS10. Tutto esatto, senza tolleranza, sui `double` dati.

**Ordine intorno a un punto** (`local_order.hpp`):

- un **mezzo pezzo**: un tratto di una retta o di un cerchio (il sostegno di
  una curva della 0.2, `Carrier`) che esce da un punto esatto P (`ExactPoint`,
  anche con radice) in uno dei due versi;
- il **confronto** di due mezzi pezzi che escono dallo stesso P: prima la
  direzione della tangente in P (in senso antiorario da una direzione fissa),
  a pari tangente la curvatura con segno (chi gira prima a sinistra), a pari
  tangente e curvatura lo stesso sostegno nello stesso verso (un tratto
  comune, che il chiamante riconosce); un ordine ciclico totale e coerente,
  anche quando P è il punto d'incontro di cerchi con radici diverse;
- l'**ordinamento** dei mezzi pezzi intorno a P, con l'indicazione dei gruppi
  uguali (tratti comuni);
- dato un mezzo pezzo «di riferimento» (per esempio l'ospite del taglio nei suoi
  due versi), in quale settore fra i mezzi pezzi ordinati cade.

**Parità rispetto a dei loop** (`winding.hpp`):

- i loop sono quelli di `region.hpp` (`Loop`, polilinea presa come chiusa o
  cerchio), già controllati dal chiamante; un loop si descrive con i suoi
  pezzi e i loro sostegni;
- lo **stato di un punto esatto** rispetto ai loop: sopra un pezzo (quale), o
  dentro o fuori con la regola pari-dispari (`region.hpp`); il punto può avere
  una radice (un incontro di una retta con un cerchio, l'estremo esatto di un
  arco);
- lo **stato dei settori** intorno a un punto dei loop: per ogni settore fra
  due mezzi pezzi consecutivi (dei loop, e del riferimento se c'è), dentro o
  fuori, con i pezzi comuni a più loop contati una volta per loop, così due
  loop che si annullano lungo un tratto lo lasciano dentro da entrambi i lati
  (ADR 0015, caso 3 e «Che cosa rende la risposta coerente», punto 2);
- il metodo è libero (raggio verso +x con i vertici e le tangenze trattati con
  l'ordine locale, somma dei settori, altro), purché la risposta sia esatta e
  non dipenda dall'ordine dei loop, dal loro verso né dal vertice iniziale di
  una polilinea (`clipping.hpp`, «The decisions … do not depend»).

Un filtro in virgola mobile per i casi lontani dalle degenerazioni è ammesso,
con il suo errore scritto accanto (come in `carriers.hpp`); la risposta resta
quella esatta.

## Regole della tappa che valgono qui

- **Contratto:** il pacchetto non tocca le intestazioni pubbliche.
- **Gli interni della 0.2** (`exact/`, `curve_meaning`, `carriers`,
  `positions`) si usano senza cambiarli; ciò che manca va nei tuoi file nuovi.
  Se serve davvero cambiarli, fermati e scrivilo nella issue.
- **Determinismo** (ADR 0008, punto 5): calcolo nei `.cpp`, niente funzioni
  trigonometriche di `<cmath>`.
- Nessuna eccezione per errori attesi; solo `std::bad_alloc`, che non si
  intercetta (ADR 0003, punto 5). Le funzioni interne non sono `noexcept`.
- I file di `tests/corpus/in-attesa/` non ti riguardano: il pacchetto non ha
  casi propri.

## Test richiesti

- `local_order_test.cpp`: quattro o più mezzi pezzi da uno stesso punto (rette e
  cerchi mescolati); tangenze interne ed esterne nello stesso punto (cerchi
  tangenti e retta tangente); cerchi con la stessa tangente e curvature
  diverse, uguali in modulo e di segno opposto; tratti comuni (stesso
  sostegno nello stesso verso); punti con radici diverse; l'ordine non cambia
  ruotando l'elenco dei mezzi pezzi o scegliendo un altro primo.
- `winding_test.cpp`: punto dentro, fuori e sopra un rettangolo, un cerchio, una
  polilinea con bulge e una che si interseca; punto sul raggio verso +x che
  passa per un vertice convesso, concavo, per una tangenza; due loop con un
  lato comune (si annullano); loop annidati; settori intorno a un vertice in cui
  si toccano due loop; le risposte non cambiano permutando i loop, invertendone
  il verso o ruotando il vertice iniziale; punti a un ulp da un lato e da un
  arco.
- Ogni risposta controllata nel test con l'aritmetica esatta, o con dati
  scelti in modo che il valore si veda a mano.

## Fatto quando

- Le due intestazioni documentano che cosa offrono, e i test le provano.
- I casi delle tappe precedenti passano senza cambiamenti.
- La CI è verde.
