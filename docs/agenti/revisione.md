# Revisione di una pull request

Si legge insieme al nucleo, [AGENTS.md](../../AGENTS.md), quando ti chiedono di
rivedere una pull request, oppure quando la sua descrizione comincia con
**Revisione del contratto: da fare**. Per rivederle di fila,
[lotti.md](lotti.md).

## Rivedere

Prendi in carico la pull request come dice AGENTS.md, "Per prima cosa: prendi in
carico", e togli l'etichetta alla fine.

Se la descrizione comincia con **Revisione del contratto: da fare**, segui
[docs/CONTRATTI.md](../CONTRATTI.md), «Le correzioni del contratto»: verbale
nella pull request, una issue `contratto` per ogni buco nuovo, nessuna modifica
alla pull request.

## La fascia

Decisione del responsabile del 2026-11-03 ([DECISIONI.md](../DECISIONI.md)).

Un caso che sta entro un errore dichiarato dal confine di `kMaxCoordinate` o da
un'altra soglia del contratto si chiude dichiarando la fascia
([ADR 0013](../adr/0013-esatto-dove-serve.md), punto 4):

- il revisore lo segnala nella revisione come **non bloccante**: non apre una
  issue `contratto`, non chiede una decisione del responsabile e non blocca il
  merge;
- l'autore scrive la fascia nella stessa pull request, nel commento
  dell'intestazione, senza un altro giro di revisione.

Vale anche dentro una revisione con la lista: un caso così non è un buco nuovo.
