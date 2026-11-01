# [0.2-P20] `geom2d`: costruzioni nei subnormali

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** #145
- **Agente:** claude
- **Difficoltà:** media (impegnativa se i casi falliscono in più funzioni)
- **Modello e impegno:** forte; impegno alto
- **Contratto:** `geom2d/include/solid_space/geom2d/curves.hpp`, `intersection.hpp`,
  `tangency.hpp`, `offset.hpp`
- **Dipende da:** C (#133, con la pull request delle decisioni unita), R20 (#146)
- **Riferimenti:** R20 (#146), codex
- **Componente:** `geom2d`
- **Da leggere:** le quattro intestazioni del contratto; [CONTRATTI.md](../../CONTRATTI.md);
  le decisioni #139 e #140 e [DECISIONI.md](../../DECISIONI.md);
  schede P10, P11, P13, P14, P15, P16 e P19 di questa tappa

La scheda recepisce il testo della issue #145, come deciso dal responsabile
nella #133. Rendere obbligatori i casi di R20 e correggere solo il codice che
non li rispetta.

## Che cosa realizza

La decisione delle #139 e #140: nei limiti di errore delle costruzioni `max(M, |p|)` diventa `max(M, |p|, 2^-1022)`, quindi nei subnormali un punto costruito sta entro 2^-1070 da quello esatto; le decisioni esatte non cambiano.

## Cosa fare

1. Cancellare `tests/corpus/in-attesa/0.2-R20.txt` e guardare quali casi falliscono.
2. **Se passano tutti**, il pacchetto è solo questo: la pull request cancella il file e lo dice.
3. **Se qualcuno fallisce**, correggere il codice senza cambiare il contratto. La causa attesa è un calcolo intermedio che va a zero o perde tutte le cifre nei subnormali (quadrati, prodotti, la radice nei centri e nei punti di tangenza, l'aritmetica a doppia precisione di `tangency.cpp`). Il rimedio consigliato è riscalare i dati di una potenza di due prima della costruzione e riportare il risultato alla scala originale alla fine: è esatto e non cambia nessuna decisione. Le decisioni esatte (`exact::`) non vanno toccate: sono già esatte.
4. Test di unità sui casi che falliscono, nei file di test dell'area corretta.

## File

Solo se servono: `geom2d/src/intersection.cpp`, `intersect_along.cpp`, `tangency.cpp`, `fillet.cpp`, `offset_curve.cpp`, `offset_polyline.cpp`, `offset_joints.cpp` e i loro test in `tests/geom2d/`; sempre `tests/corpus/in-attesa/0.2-R20.txt` (da cancellare). Nessuna intestazione.

## Fatto quando

- I casi di R20 sono obbligatori e passano; la CI è verde.
- Se un caso sembra sbagliato, ci si ferma come dice AGENTS.md («Robustezza e verifica»).

