# [0.2-P20] `geom2d`: dominio delle tangenze

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** #145
- **Agente:** claude
- **Difficoltà:** impegnativa
- **Modello e impegno:** forte; impegno alto
- **Contratto:** `geom2d/include/solid_space/geom2d/curves.hpp`, `tangency.hpp`
- **Dipende da:** C (#133, con la pull request delle decisioni unita), R20 (#146)
- **Riferimenti:** [R20](R20-dominio-tangenze.md) (#146), codex
- **Componente:** `geom2d`
- **Da leggere:** `curves.hpp` (le regole generali in testa e quelle del
  dominio) e `tangency.hpp`; [CONTRATTI.md](../../CONTRATTI.md); le decisioni
  #149–#152, #155 e #157 e [DECISIONI.md](../../DECISIONI.md);
  [ADR 0013](../../adr/0013-esatto-dove-serve.md); schede P15 e P16 di questa
  tappa

Il pacchetto nasceva per le costruzioni nei subnormali (#139, #140). L'ADR 0013
(2026-11-02) toglie i subnormali dal contratto delle costruzioni: i limiti
d'errore valgono per dati di grandezza almeno 2^-50, e sotto restano solo
niente crash, niente risultati fuori dominio e il determinismo. Resta la parte
che il codice di P15 e P16 non realizza ancora: il dominio delle costruzioni
tangenti e l'ordine dei controlli.

## Che cosa realizza

Solo dove il codice già unito non le rispetta, le decisioni:

- **#149:** dominio delle costruzioni sui valori esatti: centri, punti di
  tangenza (compreso il piede della perpendicolare della regola 1 di
  `fillet`), raggio costruito del semicerchio e punto m della forma con bulge,
  ricostruito esattamente dai double restituiti. Un valore esatto entro K non
  si restituisce oltre K: se l'arrotondamento lo supererebbe, si restituisce
  ±K. Il principio del dominio vale solo per offset e tangenze (#156). Oggi
  `tangency.cpp` controlla il dominio dei centri ma non quello dei punti di
  tangenza: va aggiunto.
- **#150:** tutti i dati prima delle costruzioni; `notCircular` resta dopo i
  dati della sua curva e prima degli ingressi seguenti. In `fillet`, regola 3,
  prima `ambiguousPick`, poi le costruzioni, `infinitelyMany` e i candidati.
  In `tangentCircleNear`, i due clic prima della diagnosi di dominio delle
  costruzioni.
- **#151:** `tangentsFrom` controlla il dominio solo dei punti elencati;
  quelli esclusi da `Extension::none` non fanno fallire la chiamata.

Non cambiano il codice, perché sono fasce (ADR 0013, punto 4), e nessun caso le
impone:

- **#152:** tangenze restituite coincidenti danno bulge 0; l'`Arc` conserva
  raggio dato e regole originarie;
- **#155 e #157:** m ricostruito al confine di K, a meno dell'errore dell'arco:
  `found` oppure `outOfDomain` sul dato 2.

## Cosa fare

1. Cancellare `tests/corpus/in-attesa/0.2-R20.txt` e guardare quali casi
   falliscono.
2. **Se passano tutti** e il codice rispetta anche #149–#151, il pacchetto
   cancella il file e lo dice.
3. **Se qualcuno fallisce**, o il codice non rispetta #149–#151, correggere il
   codice senza cambiare il contratto. Le decisioni esatte (`exact::`) non vanno
   toccate; il controllo del dominio dei punti di tangenza si fa nella stessa
   torre di numeri dei centri (`centreBeyondDomain` in `tangency.cpp`).
4. Test di unità sulle correzioni, nei file di test dell'area corretta, e un
   test per il limite ±K (un valore esatto entro K mai restituito oltre K), che
   il formato dei casi non sa esprimere.

## File

Solo se servono: `geom2d/src/tangency.cpp`, `fillet.cpp`, `tangency_internal.hpp`
e i loro test in `tests/geom2d/`; sempre `tests/corpus/in-attesa/0.2-R20.txt`
(da cancellare). Nessuna intestazione.

## Fatto quando

- I casi di R20 sono obbligatori e passano; #149–#151 sono rispettate, con il
  test del limite ±K; la CI è verde.
- Se un caso sembra sbagliato, ci si ferma come dice AGENTS.md («Robustezza e
  verifica»).
