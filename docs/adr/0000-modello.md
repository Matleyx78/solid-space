# ADR 0000: Modello di ADR

Un ADR (*Architecture Decision Record*) registra **una** decisione di struttura, con
il suo perché. Si scrive prima del codice che la attua e non si riscrive: se la
decisione cambia, se ne scrive uno nuovo che sostituisce il vecchio.

Copia questo file in `docs/adr/NNNN-titolo-breve.md` con il numero successivo e
compila le sezioni. Poche righe bastano.

## Struttura

```markdown
# ADR NNNN: Titolo

- **Stato:** Proposto | Accettato | Sostituito da ADR NNNN
- **Data:** AAAA-MM-GG

## Contesto

Che problema c'è, che cosa si sa, quali vincoli.

## Decisione

Che cosa si fa, in frasi numerate e verificabili.

## Alternative considerate

Che cosa si è scartato e perché.

## Conseguenze

Che cosa diventa più facile, che cosa più difficile, che cosa resta da fare.
```

## Stati

- **Proposto**: in discussione. Il responsabile può accettarlo quando lo ritiene
  pronto.
- **Accettato**: vale. Il codice lo rispetta.
- **Sostituito**: un ADR più recente lo rimpiazza. Si lascia dov'è e si indica il
  successore.

Un ADR accettato e non ancora attuato dal codice si può correggere, aggiungendo
nell'intestazione la riga **Corretto:** con la data e il motivo.

## Quando serve un ADR

- una nuova dipendenza esterna;
- un cambiamento delle regole di dipendenza tra componenti o dell'interfaccia
  pubblica di un componente;
- la sostituzione di un modulo di OCCT o del motore del solver;
- una convenzione che riguarda tutto il progetto;
- una richiesta di Tecnigrafo a solid-space: sempre, anche quando basta OCCT o il
  bisogno non spetta a solid-space, nel formato di
  [0000-modello-valutazione.md](0000-modello-valutazione.md)
  ([ADR 0007](0007-tappe-su-richiesta.md)).

Non serve per una funzione, un'ottimizzazione o una correzione che resta dentro un
componente e le sue interfacce.
