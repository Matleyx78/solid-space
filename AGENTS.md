# Istruzioni per agenti IA

Regole per gli assistenti IA (Claude Code, Codex, Gemini e simili) che lavorano su
solid-space; le persone le trovano spiegate in [README.md](README.md) e nei
documenti collegati. Se una richiesta dell'utente contraddice queste regole,
fermati e segnalalo invece di procedere.

solid-space è il kernel geometrico di Tecnigrafo e ne condivide metodo e regole: se
conosci [Tecnigrafo](https://github.com/Matleyx78/tecnigrafo), qui trovi le stesse
abitudini.

## Prima di scrivere codice

Leggi, in quest'ordine:

1. [docs/ROADMAP.md](docs/ROADMAP.md): quale tappa è **aperta**.
2. La **scheda** della tappa aperta, `docs/tappe/X.Y-nome.md`, e il pacchetto che
   ti è stato assegnato: file, interfacce, test e criterio di "fatto".
3. Gli ADR citati dalla scheda e [docs/ARCHITETTURA.md](docs/ARCHITETTURA.md).
4. [docs/CODING_STYLE.md](docs/CODING_STYLE.md).

Lavora **su un solo pacchetto**. Se il compito non corrisponde a un pacchetto della
tappa aperta, non scrivere codice: serve prima una decisione del responsabile. Una
buona idea fuori dalla roadmap si propone, non si implementa.

Prima di cominciare controlla che non esista già un ramo o una pull request aperta
sullo stesso pacchetto: se c'è, non toccarlo e avvisa l'utente.

## Regole che non si possono violare

- Modifica solo i file elencati nel pacchetto. Se ne serve un altro, dillo
  all'utente e spiega perché, prima di toccarlo.
- **Nessuna intestazione pubblica include OCCT, libslvs, Eigen o altre dipendenze
  esterne** (ADR 0002). Le forme e le strutture del solver sono maniglie opache.
- **Le dipendenze tra componenti vanno in un verso solo**: `features` →
  `kernel`, `naming`, `solver`; `kernel` → `robust`, `naming`; `solver` → `robust`.
  `robust` e `naming` non dipendono da nulla. Un componente non include uno che
  sta sopra di lui.
- Gli elementi si citano con identificativi stabili (`StableId`), mai con
  puntatori o indirizzi, e mai con tipi del nucleo.
- Nessuna eccezione attraversa un'interfaccia pubblica: gli errori sono valori con
  diagnosi (ADR 0003).
- Nessuna nuova dipendenza e nessuna modifica agli ADR accettati senza che il
  responsabile lo abbia deciso. Non toccare i sottomoduli in `third_party/`: sono
  fissati e non si modificano.
- Non disattivare avvisi, controlli di analisi statica o test per far passare la
  compilazione. Niente `NOLINT` senza un commento che spieghi il motivo.
- Rispetta le interfacce scritte nel pacchetto: nomi, firme e comportamenti.
- Codice, commenti e messaggi di commit in **inglese**; documenti e ADR in
  italiano.
- Ogni file di codice nuovo inizia con l'intestazione:
  ```cpp
  // SPDX-License-Identifier: GPL-3.0-or-later
  // Copyright (C) 2026 The solid-space authors
  ```
- Non copiare codice da altri progetti, a meno che l'utente non lo chieda
  esplicitamente indicando fonte, versione e licenza compatibile con la GPLv3. Usare
  OCCT e libslvs passa dai sottomoduli, non da copie.
- **Non modificare Tecnigrafo da qui.** Se un lavoro richiede un cambiamento in
  Tecnigrafo, scrivilo all'utente: lo decide il responsabile.

## Robustezza e verifica

Un pacchetto che cambia il comportamento geometrico è finito quando i test
confermano casi di verifica scritti **prima** da un agente o da una persona
**diversi** da chi scrive il codice ([ADR 0004](docs/adr/0004-robustezza-e-verifica.md)).

### In un pacchetto di lavoro

- Non modificare i casi in `tests/corpus/` che non sono del tuo pacchetto.
- Se un caso ti sembra sbagliato, **fermati subito**, anche se il resto è pronto:
  non adattare il codice al riferimento che ritieni sbagliato né il riferimento al
  codice. Scrivi all'utente il nome del caso, il valore atteso, quello che dà il
  programma e perché il riferimento ti sembra sbagliato.
- Un'operazione che può fallire restituisce un risultato con diagnosi; un ripiego
  non è mai silenzioso.

### Se scrivi i casi di riferimento

- Ricava i valori attesi da calcoli a mano, da aritmetica esatta o da file
  pubblici. **Mai eseguendo solid-space**: registreresti il suo comportamento,
  errori compresi.
- Per ogni file preso dal web registra in `sorgenti.json` indirizzo, versione,
  hash SHA-256 e licenza, compatibile con la GPLv3 e ridistribuibile. Nel dubbio,
  non usare il file e scrivilo nella pull request.
- Aggiungi i casi limite: valori nulli, punti coincidenti o quasi allineati,
  facce tangenti, spessori minimi.

## Comandi

La tappa 0.1 non ha ancora consegnato lo scheletro di compilazione: finché non c'è
(pacchetti P2 e P3 della [scheda](docs/tappe/0.1-fondamenta.md)), non esistono
comandi di build e test da eseguire. Quando ci saranno, li troverai qui, nello
stesso schema di Tecnigrafo: un solo comando locale che esegue gli stessi passi
della CI.

Se nel tuo ambiente mancano gli strumenti di build (compilatori, CMake, Ninja o
altri pacchetti di sistema), **non installarli**: un'installazione dura a lungo, può
bloccarsi e far perdere il lavoro non ancora pubblicato. Esegui i passi possibili,
pubblica il ramo e scrivi nella pull request quali passi non hai eseguito. In
particolare **non compilare OCCT** a mano: è un lavoro da decine di minuti che la
CI fa con una cache.

## Prima di dichiarare il lavoro finito

- [ ] I test richiesti dal pacchetto esistono e passano, o hai scritto nella pull
      request quali non hai potuto eseguire e perché.
- [ ] Ogni condizione "Fatto quando" del pacchetto è verificata.
- [ ] I casi di verifica non sono toccati.
- [ ] Il riepilogo per l'utente elenca i file modificati e ciò che non hai potuto
      verificare (per esempio Windows e macOS).

## Commit e pull request

- Formato Conventional Commits: `feat(solver): add constraint diagnostics`.
- Non firmare tu il Developer Certificate of Origin (niente `git commit -s`): la
  firma `Signed-off-by` la aggiunge la persona responsabile, che ne risponde.
- Una pull request per pacchetto, partita da `main` aggiornato.
- Nella descrizione della pull request: che cosa cambia, quali file, che cosa non
  hai potuto verificare.
