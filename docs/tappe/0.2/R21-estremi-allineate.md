# [0.2-R21] Riferimenti: estremi delle linee allineate unite

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** da aprire con l'unione della pull request del contratto (#158)
- **Agente:** codex
- **Difficoltà:** media
- **Modello e impegno:** forte; impegno alto
- **Riferimenti del pacchetto:** [P21](P21-estremi-allineate.md)
- **Contratto:** `geom2d/include/solid_space/geom2d/coincidence.hpp`
  (`joinAligned`)
- **Dipende da:** P1 (#34)
- **Sblocca in Tecnigrafo:** Matleyx78/tecnigrafo#372 (R22, i riferimenti
  dell'unione, con R4b, R8a e R8b), che prende da qui i valori geometrici
- **Da leggere:** [ADR 0009](../../adr/0009-regola-delle-tolleranze-del-piano.md)
  (riga **Corretto:** del 2026-11-02, punto 5); il commento di `joinAligned`
  in `coincidence.hpp` e «Checking the data» in `curves.hpp`; la decisione #158
  in [DECISIONI](../../DECISIONI.md); il
  [README dell'archivio](../../../tests/corpus/README.md) e AGENTS.md

Non realizza P21 e non modifica il codice.

## Vocabolario

| Operazione | `ingresso` | `atteso` |
|---|---|---|
| `joinAligned` | `sorgente` (un `segmento`), `altri` (una lista di `segmento`, anche vuota) | `curva` (un `segmento`), oppure `diagnosi` |

## Cosa verificare

- L'esempio della #158, in millimetri: sorgente (0,0)→(10,0), altro
  (0,5e-7)→(5,0): (0,0)→(10,0).
- **Pareggi esatti** (stessa proiezione esatta lungo la sorgente):
  - tra la sorgente e un altro, all'inizio e alla fine: vince la sorgente;
  - tra due altri, all'inizio e alla fine: vince il primo nell'ordine; lo
    stesso caso con gli altri in ordine inverso;
  - tra inizio e fine dello stesso altro (un segmento perpendicolare alla
    sorgente): vince l'inizio;
  - con la sorgente nei due versi: il risultato è lo stesso invertito.
- **A un ulp:** due estremi con proiezioni che differiscono di pochissimo
  (un estremo spostato di un ulp lungo la sorgente, o una sorgente obliqua in
  cui la differenza esatta è minima ma non nulla): vince il più avanti, non il
  primo nell'ordine.
- Spazi tra i segmenti; altri che vanno oltre l'inizio della sorgente o la
  contengono; altri nel verso opposto alla sorgente; nessun altro (la
  sorgente).
- Coordinate di 1e6 e una sorgente obliqua, dove le proiezioni non sono
  rappresentabili in `double`: la risposta resta quella esatta.
- Segmenti non allineati: la risposta segue la regola anche per loro (la
  funzione non controlla l'allineamento).
- **Diagnosi:** sorgente ridotta a un punto (`degenerateCurve`, dato 0); un
  altro non valido, fuori dominio o ridotto a un punto (dato i + 1); due
  ingressi sbagliati insieme (vince il dato più piccolo).

## File

Solo `tests/corpus/`: casi nuovi in `casi/geom2d/` e
`in-attesa/0.2-R21.txt` (i casi nuovi, con la issue di P21).

## Valori attesi e fatto quando

Calcoli a mano o `fractions` di Python, mai eseguendo solid-space: ogni
`double` è un razionale e ogni proiezione è esatta. Documentare per ogni caso
le proiezioni confrontate e il pareggio. Se il contratto non determina un
caso, fermarsi secondo AGENTS.md. I casi coprono le voci sopra, passano la
validazione e sono pubblicati separatamente dal lavoro P21, con CI verde.
