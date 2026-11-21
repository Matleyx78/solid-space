# Revisione di una pull request

Si legge insieme al nucleo, [AGENTS.md](../../AGENTS.md), quando ti chiedono di
rivedere una pull request, oppure quando la sua descrizione comincia con
**Revisione del contratto: da fare**. Nella catena degli agenti il verbale ha
anche i giri e le classi di [catena.md](catena.md), "Revisione (revisore)":
[bloccante], [decisione] per i buchi visibili, [fascia] per «La fascia» qui
sotto, [facoltativa] per ciò che qui è «non bloccante».

## Rivedere

Prendi in carico la pull request come dice AGENTS.md, "Per prima cosa: prendi in
carico", e togli l'etichetta alla fine.

Se la descrizione comincia con **Revisione del contratto: da fare**, segui
[docs/CONTRATTI.md](../CONTRATTI.md), «Le correzioni del contratto»: verbale
nella pull request, una issue `contratto` per ogni buco nuovo (nella catena, una
segnalazione [decisione] nel verbale), nessuna modifica alla pull request.

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

Il revisore usa un **modello diverso** da quello dell'autore, che la pull
request dichiara nella sezione sull'uso dell'IA: se è il suo stesso modello,
non la rivede e lo dice al responsabile (nella catena, con [catena.md](catena.md),
"Fermarsi per una decisione"). Il suo verbale dice il modello usato
([DECISIONI.md](../DECISIONI.md), 2026-11-07).

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

## Revisione del piano

Prima dell'apertura di una tappa (guida del responsabile, B4a, con questo
repository), l'agente che scriverà la maggior parte dei riferimenti rilegge
**il piano intero**, con le schede e le prove a secco, in fascia forte. Prende
in carico, non modifica la pull request del piano, scrive **un solo verbale**
con [bloccante], [decisione] e [facoltativa]. **C'è un solo giro**: le
[bloccante] le corregge chi ha scritto il piano, le [decisione] le prende il
responsabile, le [facoltativa] non si trattano. Per ogni scheda controlla:

1. **Raggiungibilità:** ogni "Fatto quando" e ogni diagnosi si raggiunge con le
   sole dipendenze del pacchetto.
2. **Strumenti:** la prova a secco si scrive con il formato e il validatore di
   `main` o dell'onda 0.
3. **Fonte:** ogni comportamento atteso ha la sua fonte, l'ADR di valutazione,
   l'intestazione o la richiesta di Tecnigrafo; altrimenti è un buco del
   contratto.
4. **Trasversali:** sono segnati e stanno in testa, o in coda con il motivo e
   l'elenco dei casi.
5. **Regole numeriche:** per le schede che confrontano o scelgono fra valori
   quasi uguali, la regola c'è nell'[ADR 0013](../adr/0013-esatto-dove-serve.md)
   o nell'intestazione che la scheda usa; altrimenti è un buco del contratto,
   cioè una [decisione].
6. **Risposte condivise:** la riga della prova a secco c'è, e dice quali casi di
   Tecnigrafo cambiano e quando li aggiorna il sottomodulo.

Guarda anche la larghezza delle onde, fino a 8 pacchetti indipendenti
([DECISIONI.md](../DECISIONI.md), 2026-11-21).
