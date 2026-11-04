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

## A che cosa serve la revisione

Decisione del responsabile del 2026-11-04 ([DECISIONI.md](../DECISIONI.md)).

Il revisore serve a far arrivare il lavoro al merge, non a fermarlo. Blocca solo
un **errore vero** o una **regola persa**. Se una regola esistente risponde, la
applica e va avanti; i dubbi di forma li scrive come **non bloccanti**.

## Prima di dichiarare un buco

Prima di dichiarare un buco, il revisore controlla se una regola esistente lo
decide già, in particolare:

- **dati fuori dominio**: [ADR 0035](https://github.com/Matleyx78/tecnigrafo/blob/main/docs/adr/0035-taglio-offset-raccordi-unione.md)
  di Tecnigrafo, punto 23;
- **pezzi degeneri**: [ADR 0036](https://github.com/Matleyx78/tecnigrafo/blob/main/docs/adr/0036-tolleranze-di-tecnigrafo.md)
  di Tecnigrafo, punto 5;
- **fasce**: [ADR 0013](../adr/0013-esatto-dove-serve.md), punto 4.

Se una di queste regole lo decide, il revisore lo scrive come **applicazione di
quella regola**, non come buco, e non blocca il merge. Dati privi di senso
pratico (lunghezze sotto la tolleranza, ampiezze di un ulp) si trattano con
queste regole.

## Test, non dimostrazioni

Decisione del responsabile del 2026-11-04 ([DECISIONI.md](../DECISIONI.md)).

Nelle revisioni una garanzia del contratto si verifica **con i test**, non con
dimostrazioni matematiche. Il revisore che teme un ramo non coperto chiede un
**test che lo raggiunga**, non un lemma.
