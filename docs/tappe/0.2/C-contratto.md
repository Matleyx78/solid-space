# [0.2-C] Revisione del contratto: `offset.hpp` e `tangency.hpp`

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** #133
- **Difficoltà:** impegnativa
- **Agente:** codex
- **Modello e impegno:** forte; impegno alto
- **Riferimenti:** non servono: non cambia il comportamento
- **Contratto:** `geom2d/include/solid_space/geom2d/offset.hpp`,
  `geom2d/include/solid_space/geom2d/tangency.hpp`; regole generali nei commenti
  di `geom2d/include/solid_space/geom2d/curves.hpp` (#139–#142)
- **Dipende da:** —
- **Componente:** nessuno (solo documenti e commenti delle intestazioni)
- **Da leggere:** [CONTRATTI.md](../../CONTRATTI.md), tutto, con i punti 10 e 11
  e «Le correzioni del contratto»;
  [ADR 0010](../../adr/0010-posizione-e-intersezioni-nel-piano.md),
  [ADR 0011](../../adr/0011-raccordi-e-tangenze-nel-piano.md) e
  [ADR 0012](../../adr/0012-curva-parallela-nel-piano.md), con le righe
  **Corretto:**; [DECISIONI.md](../../DECISIONI.md), le voci dal 2026-10-31;
  le due intestazioni in `main`; la issue #132, ancora aperta

## Perché

La tappa 0.2 è partita prima che esistesse la revisione del contratto, che
vale dalla tappa successiva. Le due intestazioni più cambiate dopo
l'accettazione sono state corrette sei volte, sempre da decisioni applicate
senza ripassare la lista:

- `offset.hpp`: pull request #97, #99, #107 (issue #103) e #128 (issue #127);
- `tangency.hpp`: pull request #122 (issue #66) e #126 (issue #70).

La #128 ha aperto un buco nuovo (#132), trovato da R19 cinquanta minuti dopo il
merge. Questo pacchetto fa per le due intestazioni la revisione che la tappa non
ha avuto, sul testo di oggi, prima che R19, P19 e i pacchetti di Tecnigrafo che
le usano (P17–P21 della sua tappa 0.7) partano o riprendano.

## Cosa fare

È una revisione del contratto come in [CONTRATTI.md](../../CONTRATTI.md), «La
revisione del contratto», limitata alle due intestazioni:

1. Per ogni funzione e ogni tipo restituito di `offset.hpp` e `tangency.hpp`,
   ogni punto della lista, **1–11**: determinato (e dove: intestazione, ADR,
   scheda, decisione) oppure **buco**. I punti 7, 10 e 11 sono quelli in cui
   sono nati gli ultimi buchi: cercali con esempi al limite (archi quasi
   interi, corde di pochi ulp, coordinate vicine a `kMaxCoordinate`, punti a
   meno di un ulp).
2. La issue #132 è già aperta: citala nel verbale, non aprirne un'altra sullo
   stesso punto. Se trovi che la sua decisione ne tocca altri, scrivilo nel
   verbale.
3. Per il punto 11 puoi leggere, solo in lettura, come le usa Tecnigrafo: le
   schede P17–P21 della sua tappa 0.7 (`docs/tappe/0.7/` in
   Matleyx78/tecnigrafo) e la sua facciata. Non scrivere in Tecnigrafo: se una
   scheda di Tecnigrafo perde un'informazione che il contratto dà, scrivilo nel
   verbale, e lo riferisce il responsabile.
4. Per ogni buco, una issue con le etichette `contratto` e `tappa-0.2`,
   intitolata `[0.2-C] <intestazione>: <funzione>: <punto>`, con che cosa
   manca, un esempio con i dati, le scelte possibili e quella che consigli.
5. Il verbale: un commento nella issue di questo pacchetto, con la lista
   spuntata per ogni funzione e i numeri delle issue aperte. Poi togli
   `AA-Presa in carico` e fermati: decide il responsabile.

Dopo le decisioni, in una nuova sessione, una sola pull request le applica
(commenti delle intestazioni, riga **Corretto:** negli ADR, schede interessate,
[DECISIONI.md](../../DECISIONI.md), «Modifiche al piano»), con **Revisione del
contratto: da fare** in testa: la rivede un agente diverso (CONTRATTI.md, «Le
correzioni del contratto»). Se non ci sono buchi, basta il verbale e il
responsabile chiude la issue.

## File

- Durante la revisione: nessuno (issue e verbale).
- Dopo le decisioni: `geom2d/include/solid_space/geom2d/offset.hpp` e
  `tangency.hpp` (solo commenti, e dichiarazioni solo se la decisione lo dice),
  `geom2d/include/solid_space/geom2d/curves.hpp` (solo commenti: ordine delle
  diagnosi, dominio dei dati costruiti e pavimento 2^-1022 di tutti i limiti
  d'errore, decisioni #136–#144 e commenti del responsabile nella #133),
  `docs/adr/0010-posizione-e-intersezioni-nel-piano.md`,
  `docs/adr/0011-raccordi-e-tangenze-nel-piano.md`,
  `docs/adr/0012-curva-parallela-nel-piano.md`, le schede dei pacchetti
  interessati in `docs/tappe/0.2/`, `docs/DECISIONI.md`,
  `docs/tappe/0.2-geometria-del-piano.md`. Questa scheda autorizza a cambiare
  le intestazioni solo come decide il responsabile (ADR 0007, punto 15).
  Comprende le schede R19/P19 allargate, la nuova scheda P20 da #145, le righe
  R20 (#146) e P20 (#145) nel piano e la dipendenza di P18 da P20.
  Comprende le decisioni #148–#152 e le correzioni del responsabile dopo il
  verbale: solo dominio, controllo esatto e limite dell'arrotondamento,
  ordine dei controlli, punti esclusi e bulge puntiforme; schede P7, P10 e P11
  già chiuse invariate, perché vale la regola generale di `curves.hpp`.

## Fatto quando

- Il verbale copre ogni funzione e ogni punto 1–11 delle due intestazioni.
- Ogni buco ha la sua issue `contratto`.
- Se ci sono buchi: la pull request che applica le decisioni ha il verbale della
  revisione delle correzioni e la CI verde.
