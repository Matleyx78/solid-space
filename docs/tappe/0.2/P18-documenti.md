# [0.2-P18] Documenti: determinismo e stato del contratto

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** #72
- **Difficoltà:** facile
- **Agente:** gemini
- **Modello e impegno:** leggero; impegno basso
- **Riferimenti:** non servono: documentazione e commenti, nessun cambiamento di
  comportamento
- **Dipende da:** P8 (#49), P11 (#56), P14 (#63), P16 (#70), P17 (#71), P19 (#130),
  P20 (#145): i primi quattro chiudono le catene che realizzano le intestazioni del contratto, P17 è
  l'ultimo altro pacchetto che tocca `tests/CMakeLists.txt`; P19 completa
  bulge e vertici dell'offset, P20 le costruzioni nei subnormali
- **Sblocca in Tecnigrafo:** niente
- **Componente:** documenti (più i commenti iniziali delle intestazioni del
  contratto)
- **Da leggere:** [ADR 0008](../../adr/0008-componente-geom2d.md) punti 5 e 6,
  [ADR 0009](../../adr/0009-regola-delle-tolleranze-del-piano.md) punto 1,
  [ADR 0010](../../adr/0010-posizione-e-intersezioni-nel-piano.md) punti 2–4;
  `docs/CODING_STYLE.md` ("Geometria"), `docs/ARCHITETTURA.md` ("Regole che
  valgono ovunque", "Dove si trova cosa"), `README.md` ("Componenti"); la
  sezione 6 del [piano](../0.2-geometria-del-piano.md), per sapere che i pacchetti
  sono uniti

## File

- `docs/CODING_STYLE.md`
- `docs/ARCHITETTURA.md`
- `README.md`
- i commenti iniziali delle 13 intestazioni del contratto:
  `robust/include/solid_space/robust/` (`diagnosis.hpp`, `point.hpp`,
  `predicates.hpp`, `result.hpp`, `tolerance.hpp`, `trigonometry.hpp`) e
  `geom2d/include/solid_space/geom2d/` (`coincidence.hpp`, `curves.hpp`,
  `diagnosis.hpp`, `intersection.hpp`, `offset.hpp`, `side.hpp`, `tangency.hpp`)
- i commenti «draft contract» di `tests/CMakeLists.txt`,
  `tests/robust/contract_test.cpp` e `tests/geom2d/contract_test.cpp`

Sono più della quindicina di file di un pacchetto, ma in sedici di essi cambiano
solo una o due righe di commento.

## Cosa fare

1. **`CODING_STYLE.md`, sezione "Geometria":** le regole di `robust` e `geom2d`
   per chi scrive codice, in breve e con il rimando agli ADR, senza ripeterli:
   - tutto il calcolo in virgola mobile nei `.cpp`, mai in funzioni `inline` o
     `constexpr` delle intestazioni pubbliche (ADR 0008, punto 5.1);
   - di `<cmath>` solo le funzioni arrotondate correttamente (`sqrt`, `fma`,
     `fabs`…); seni, coseni e angoli da `robust` (punto 5.3);
   - decisioni esatte, costruzioni con l'errore dichiarato nell'intestazione,
     tolleranze solo nelle funzioni che le nominano (punto 6; ADR 0009, punto 1);
   - il calcolo esatto non dipende dall'intervallo degli esponenti dei `double`
     (piano della tappa 0.2, sezione 4).
2. **`README.md`, sezione "Componenti":** una frase per chi incorpora la
   libreria: niente `-ffast-math` né `/fp:fast` ai target di solid-space, solo
   piattaforme a 64 bit, e la configurazione lo controlla (ADR 0008, punti 5.4 e
   5.5; P3).
3. **`ARCHITETTURA.md`, "Dove si trova cosa":** la frase sulle intestazioni del
   contratto «in bozza e non ancora realizzate» diventa: realizzate con la tappa
   0.2; e una riga che dice dove stanno le parti interne di `geom2d`
   (`geom2d/src/exact/`, le intestazioni private in `geom2d/src/`).
4. **Le intestazioni del contratto:** solo il commento iniziale. «Draft contract
   (ADR 0007, point 15): see ADR …» diventa «Contract with Tecnigrafo (ADR 0007,
   point 15): see ADR …», e la frase «Declarations only: nothing here may be used
   before the package that implements it is merged» (o la sua variante in
   `point.hpp`) si toglie. Nessun'altra riga cambia: né dichiarazioni, né altri
   commenti. È l'unica modifica alle intestazioni del contratto che il piano
   autorizza (sezione 3).
5. **I test del contratto:** nei tre file di test, «draft contract» e «nothing
   that is not implemented yet» diventano commenti che dicono che quei test
   controllano le firme del contratto; il codice dei test non cambia.

## Regole della tappa che valgono qui

- Documenti in italiano, commenti del codice in inglese.
- Prima di cominciare controlla che le issue di P8, P11, P14, P16, P17, P19 e P20 siano
  chiuse: la frase del punto 3 e i commenti del punto 4 sono veri solo dopo.
- Nelle intestazioni, se una riga da cambiare non è come la descrive questa
  scheda, fermati e scrivilo nella issue.
- `clang-format` sulle intestazioni toccate (`scripts/controlla.sh --solo
  formato`).

## Test richiesti

- Nessun test nuovo; `scripts/controlla.sh` passa (formato e compilazione delle
  intestazioni).
- `git diff` sulle intestazioni e sui tre file di test mostra solo righe di
  commento.

## Fatto quando

- `CODING_STYLE.md`, `README.md` e `ARCHITETTURA.md` dicono quanto sopra.
- Le 13 intestazioni del contratto non si dicono più in bozza, e nient'altro è
  cambiato in esse.
- La CI è verde.
