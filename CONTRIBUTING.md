# Come contribuire

Grazie dell'interesse. solid-space cresce per **tappe** pianificate dal
responsabile del progetto: chi contribuisce non deve inventare cosa fare, ma
sceglie un compito già definito nei dettagli. Leggi tutto questo file prima di
iniziare: le pull request che non lo rispettano vengono chiuse senza revisione.

## In cinque minuti

1. Apri [docs/ROADMAP.md](docs/ROADMAP.md) e guarda quale tappa è **aperta**.
2. Apri il documento della tappa in `docs/tappe/`: contiene i **pacchetti di
   lavoro**. Ognuno ha una scheda che dice quali file toccare, quali interfacce
   rispettare, quali test scrivere e quando è "fatto".
3. Su GitHub, nel milestone della tappa, scegli la issue di un pacchetto libero.
   Se è la prima volta, cerca l'etichetta `good first issue`.
4. Scrivi nella issue che vuoi occupartene e **aspetta l'assegnazione**.
5. Lavora, apri la pull request, aspetta la revisione.

Come si leggono i riferimenti (`[0.2-P3]`, `#5`, `Closes #5`) e come lavorano più
persone o agenti in parallelo: [docs/LAVORO-IN-PARALLELO.md](docs/LAVORO-IN-PARALLELO.md).

Un pacchetto che cambia il comportamento geometrico ha anche un pacchetto di
**riferimenti** (`[0.2-R3]`): casi di verifica scritti da qualcun altro prima del
lavoro, che i test confrontano con il codice
([ADR 0004](docs/adr/0004-robustezza-e-verifica.md)). Una pull request di lavoro
non li modifica mai: se un caso ti sembra sbagliato, scrivilo nella issue.

## Il percorso di una modifica

1. **Assegnazione.** Un pacchetto è assegnato a una sola persona alla volta. Se
   non dai notizie per 14 giorni, il pacchetto torna libero.
2. **Ramo.** Crea un ramo dal nome `pX.Y-NN-descrizione` (esempio:
   `p0.2-03-orient2d`).
3. **Pull request.** Una PR realizza **un solo pacchetto** e cita la sua issue
   (`Closes #123`). Niente modifiche "già che c'ero": se trovi altro da sistemare,
   apri una issue a parte.
4. **CI verde.** Quando la CI esiste (tappa 0.1), formattazione, compilazione
   senza avvisi su tre sistemi, analisi statica e test devono passare. Una PR con
   la CI rossa non viene revisionata.
5. **Revisione.** Almeno un'approvazione del responsabile o di un revisore
   dell'area. Le osservazioni si risolvono con nuovi commit.
6. **Merge.** Lo fa il revisore, con *squash* in un unico commit.

### Cosa si può fare senza un pacchetto

- Correggere un errore di una tappa chiusa: apri prima una issue.
- Migliorare documentazione e test esistenti.
- Proporre un'idea per le tappe future con una issue *Idea* (etichetta `idea`): si
  discute, ma non si scrive codice finché il responsabile non la inserisce in una
  tappa.

Nient'altro, anche se il codice è ben scritto.

### Se il piano di un pacchetto è sbagliato

Capita. Non aggirarlo nel codice: scrivi nella issue cosa non funziona e proponi
una correzione. Il responsabile aggiorna il piano della tappa e la sezione
"Modifiche al piano".

## Regole tecniche obbligatorie

### Architettura

- Rispetta gli ADR in `docs/adr/` e la mappa in
  [docs/ARCHITETTURA.md](docs/ARCHITETTURA.md). Se non sei d'accordo con un ADR,
  proponi un nuovo ADR; non aggirarlo nel codice.
- Dipendenze a senso unico tra i componenti. Nessuna intestazione pubblica mostra
  OCCT, libslvs o altre dipendenze esterne.
- Nessuna nuova dipendenza esterna senza un ADR approvato.

### Codice

- C++20. Stile descritto in [docs/CODING_STYLE.md](docs/CODING_STYLE.md).
- Codice, commenti, nomi e messaggi di commit in **inglese**, per permettere a
  chiunque nel mondo di leggerli. Documenti e ADR sono in italiano.
- Formattazione con `clang-format` 18 usando il `.clang-format` del progetto
  (dal pacchetto P3 della tappa 0.1).
- Nessun avviso del compilatore e nessuna segnalazione di `clang-tidy`.
- Ogni file sorgente inizia con l'intestazione SPDX della licenza.

### Test

- Ogni modifica a un componente arriva con i suoi test. Una correzione di bug
  arriva con il test che la riproduce.
- Un test che prova un comportamento geometrico usa un caso di `tests/corpus/`, non
  valori ricavati eseguendo il codice.
- I test non dipendono dall'ordine di esecuzione, dalla rete o dall'ora.

### Commit

- Formato [Conventional Commits](https://www.conventionalcommits.org/):
  `feat(solver): add constraint diagnostics`, `fix(robust): handle collinear input`.
- Ogni commit di una persona è firmato con `git commit -s` (riga
  `Signed-off-by`): con questa firma dichiari che il codice è tuo o che hai il
  diritto di contribuirlo sotto GPLv3 (Developer Certificate of Origin,
  https://developercertificate.org). Gli agenti IA non firmano: la firma la
  aggiunge il responsabile allo squash.
- Non incollare codice da altri progetti senza indicarne la provenienza e la
  licenza nella PR. Codice con licenza incompatibile con la GPLv3 non è accettato.

## Contribuire con l'aiuto di un'IA

È permesso, alle stesse condizioni di qualunque altro contributo, più tre:

1. **Una persona ne risponde.** Chi apre la PR ha letto e capito ogni riga, e
   risponde alle domande della revisione senza rimandare all'IA.
2. **Si dichiara.** Ogni commit scritto con un'IA porta una riga di attribuzione
   (`Assisted-by: <strumento>`, oppure la riga `Co-Authored-By` che lo strumento
   aggiunge da solo), e la PR compila la sezione apposita del modello.
3. **L'IA segue le stesse regole.** Le istruzioni per gli agenti sono in
   [AGENTS.md](AGENTS.md): gli strumenti più diffusi lo leggono da soli; per gli
   altri, fallo leggere all'inizio della sessione insieme al pacchetto di lavoro.

Le PR generate in massa, che non rispettano il pacchetto o che il proponente non
sa spiegare, vengono chiuse.

## Prima di aprire la pull request

Formattazione, compilazione e test si lanciano con **un solo comando locale**, che
esegue gli stessi passi della CI. Arriva con i pacchetti P2 e P3 della
[tappa 0.1](docs/tappe/0.1-fondamenta.md) e sarà descritto in
[AGENTS.md](AGENTS.md), sezione "Comandi". Gli strumenti da installare sono in
[docs/AMBIENTE-SVILUPPO.md](docs/AMBIENTE-SVILUPPO.md).
