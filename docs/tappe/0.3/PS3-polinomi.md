# [0.3-PS3] `geom2d`: radici dei polinomi fino al quarto grado (interno)

- **Tappa:** 0.3 — [piano](../0.3-regioni-ed-ellisse.md)
- **Issue:** #…
- **Difficoltà:** impegnativa
- **Agente:** claude
- **Modello e impegno:** forte; impegno alto. Resta impegnativo: è uno
  strumento nuovo senza uno schema nel codice, con radici multiple e
  coefficienti lunghi, e nessun riferimento lo copre prima di PS7 e PS8
- **Riferimenti:** non servono: nessuna funzione pubblica; lo provano i test di
  unità con l'aritmetica esatta, e attraverso le funzioni pubbliche i casi di
  RS7a–RS7c e RS8. La pull request la rivede Codex, in fascia forte
- **Contratto:** nessuna
- **Dipende da:** —
- **Sblocca in Tecnigrafo:** niente da solo; serve a PS7 e PS8 (PT50)
- **Componente:** `geom2d` (interno)
- **Da leggere:** [ADR 0017](../../adr/0017-ellisse-nel-piano.md) («Che cosa
  rende possibili decisioni esatte» e il rischio in «Conseguenze»);
  `geom2d/src/exact/dyadic.hpp`, `quadratic.hpp` e `tower.hpp` (tappa 0.2), che
  usi senza cambiarli

## File

- `geom2d/src/exact/polynomial.hpp`, `geom2d/src/exact/polynomial.cpp` (nuovi)
- `tests/geom2d/polynomial_test.cpp` (nuovo)

Nessun `CMakeLists.txt`: la raccolta prende anche `src/exact/*.cpp`.

## Perché serve

L'ellisse incontra un cerchio o un'altra ellisse nelle radici reali di un
polinomio di grado al più 4 nel parametro della sua forma razionale, e i piedi
delle perpendicolari da un punto sono ancora le radici di un polinomio di
quarto grado (ADR 0017). Queste radici non stanno in generale in una torre di
radici quadrate (`exact/tower`), quindi le decisioni (quante radici, con quale
molteplicità, dove stanno rispetto agli estremi di un arco, in che ordine)
chiedono uno strumento nuovo. Sta in un pacchetto a sé perché non dipende
dall'ellisse: parte subito, in parallelo a PS4, e PS7 e PS8 lo usano senza
cambiarlo.

## Cosa fare

Un'intestazione interna, `exact/polynomial.hpp`, nel namespace
`solid_space::geom2d::exact`. Nomi e firme li fissi tu; l'intestazione li
documenta, perché li usano PS7 e PS8.

- **Polinomi** di grado al più 4 a coefficienti diadici esatti (`Dyadic`), con
  somma, prodotto, derivata, valutazione esatta in un diadico; grado e
  coefficiente direttore esatti (un coefficiente nullo abbassa il grado).
- **Parte senza fattori ripetuti** e **molteplicità** di ogni radice reale, dal
  massimo comun divisore con la derivata (resti con pseudo-divisione, senza
  frazioni che crescono senza controllo).
- **Successione di Sturm**: il numero esatto di radici reali distinte in un
  intervallo con estremi diadici, e su tutta la retta.
- **Isolamento**: un elenco ordinato di intervalli disgiunti a estremi diadici,
  uno per radice reale distinta, con la sua molteplicità.
- **Segno di un altro polinomio in una radice** (di grado al più 4, o al più
  quello che chiede PS7 per le posizioni sugli archi: scrivi il limite
  nell'intestazione): esatto, anche quando è zero (radice comune, dal massimo
  comun divisore).
- **Confronto di due radici** di polinomi diversi: uguali, prima, dopo; esatto.
- **Restringimento** di un intervallo fino a una larghezza data, per costruire i
  punti entro gli errori dichiarati dei chiamanti; un arrotondamento a `double`
  di una radice con un errore scritto.
- Esatto per ogni esponente, come i numeri di P6 della 0.2: nessuna soglia, e il
  costo resta limitato perché il grado è limitato.

Un filtro in virgola mobile (intervalli calcolati in `double` con l'errore
scritto) è ammesso per i casi lontani dalle radici multiple; la risposta resta
quella esatta.

## Regole della tappa che valgono qui

- **Contratto:** il pacchetto non tocca le intestazioni pubbliche.
- **Gli interni della 0.2** (`exact/dyadic`, `quadratic`, `tower`) si usano
  senza cambiarli; se manca qualcosa, va nei tuoi file nuovi, o ti fermi e lo
  scrivi nella issue.
- **Determinismo** (ADR 0008, punto 5): calcolo nei `.cpp`, niente funzioni
  trigonometriche di `<cmath>`.
- Nessuna eccezione per errori attesi; solo `std::bad_alloc`.
- I file di `tests/corpus/in-attesa/` non ti riguardano.

## Test richiesti

- `polynomial_test.cpp`: polinomi con quattro radici reali distinte, con due
  radici doppie, con una radice tripla e una quadrupla, con radici complesse;
  radici vicinissime (che differiscono di 2^-60) separate; coefficienti
  diadici con esponenti molto diversi (verso 2^-1074 e verso 2^200); grado che
  scende per un coefficiente direttore nullo; segno di un altro polinomio in una
  radice comune (zero) e in una radice vicina; confronto di radici uguali di
  polinomi diversi (per esempio p e p·q); restringimento fino all'errore
  chiesto; ogni risposta controllata con valori noti (radici razionali,
  √2 e simili, prodotti di fattori noti).

## Fatto quando

- `exact/polynomial.hpp` documenta che cosa offre, e i test lo provano.
- I casi delle tappe precedenti passano senza cambiamenti.
- La CI è verde.
