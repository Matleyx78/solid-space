# Governo del progetto

solid-space nasce per dare un kernel geometrico robusto a Tecnigrafo e a chiunque
voglia usarlo, evitando l'errore più comune dei grandi progetti liberi: crescere in
mille direzioni, una per ogni persona che contribuisce. Qui la direzione è una sola
e le regole valgono per tutti, fondatore compreso.

## Ruoli

**Responsabile del progetto (maintainer principale).** È il fondatore. Ha l'ultima
parola su architettura, priorità, stile, dipendenze e su cosa entra nel codice. Può
delegare, ma non rinunciare, a questa responsabilità. Se un giorno passerà il ruolo a
qualcun altro, lo farà per iscritto in questo file.

**Revisori.** Persone nominate dal responsabile, che hanno dimostrato nel tempo di
conoscere e rispettare le regole. Possono approvare modifiche nelle aree a loro
assegnate (file `.github/CODEOWNERS`). Il responsabile può revocare la nomina.

**Contributori.** Chiunque proponga una modifica seguendo
[CONTRIBUTING.md](CONTRIBUTING.md).

## Principi

1. **Una sola visione.** Le funzioni nascono dalla roadmap, non dalle preferenze di
   chi le scrive. Una buona idea fuori dalla roadmap si propone, non si implementa.
2. **Prima il progetto, poi il codice.** Il progetto avanza per tappe, una alla
   volta ([docs/ROADMAP.md](docs/ROADMAP.md)). Le tappe e i loro pacchetti di lavoro
   li scrive e li pubblica il responsabile; le scelte architetturali si scrivono in
   un ADR (`docs/adr/`).
3. **Le regole le fanno rispettare gli strumenti.** Formattazione, avvisi del
   compilatore, analisi statica e test girano in CI. Una pull request con la CI
   rossa non viene nemmeno revisionata.
4. **Qualità prima della velocità.** Meglio una funzione in meno che una funzione
   fragile. "No" e "non ora" sono risposte valide e non richiedono giustificazioni
   lunghe.
5. **Il codice è di tutti, la direzione no.** Il software è libero (GPLv3): chi non
   condivide la direzione è libero di fare un fork. Non è libero di trascinare
   questo progetto altrove.

## Rapporto con Tecnigrafo

solid-space è il kernel di [Tecnigrafo](https://github.com/Matleyx78/tecnigrafo) e
ne condivide metodo e regole ([ADR 0001](docs/adr/0001-scopo-e-confini.md)), ma
**vive anche da solo**: ha il suo repository, la sua roadmap, i suoi ADR e le sue
regole di lavoro, e si compila e si prova senza Tecnigrafo.

- I due progetti hanno lo stesso responsabile. Le priorità di solid-space seguono
  il calendario di Tecnigrafo; quando Tecnigrafo lo cambia, il responsabile
  aggiorna la tabella della [roadmap](docs/ROADMAP.md).
- Le interfacce tra i due si decidono insieme, con un ADR per parte quando serve.
- **Ognuno scrive solo a casa sua.** Chi lavora a solid-space (persone o agenti)
  scrive codice e documenti solo in solid-space. In Tecnigrafo può leggere e, su
  richiesta del responsabile e indicando la issue o la pull request, scrivere un
  commento: nient'altro. Se un lavoro richiede un cambiamento in Tecnigrafo, lo
  segnala al responsabile, che decide. Il modo di farlo rispettare agli agenti è
  in [AGENTS.md](AGENTS.md), "Dove puoi scrivere".

## Come si decide

- **Tappe:** il responsabile scrive la scheda, poi il piano dettagliato con i
  pacchetti di lavoro, apre la tappa e la chiude quando i criteri sono verificati
  (regole in [docs/ROADMAP.md](docs/ROADMAP.md)). Il piano si può discutere
  pubblicamente prima dell'apertura.
- **Lavoro ordinario** (pacchetti della tappa aperta, correzioni, test): issue
  assegnata → pull request → revisione → merge
  ([docs/LAVORO-IN-PARALLELO.md](docs/LAVORO-IN-PARALLELO.md)).
- **Decisioni architetturali** (nuovi componenti, dipendenze, formati di file,
  modifiche agli ADR): proposta scritta come ADR in stato *Proposto*, discussa
  pubblicamente. L'accettazione è decisione del responsabile, che la prende quando
  ritiene l'ADR pronto; non può invece respingerlo prima di 7 giorni dalla
  proposta. L'ADR passa in stato *Accettato* o *Respinto* e resta nell'archivio in
  ogni caso.
- **Cambio di un ADR accettato:** si scrive un nuovo ADR che lo sostituisce. Gli ADR
  non si riscrivono. Eccezione: un ADR accettato che nessun codice unito attua
  ancora (di solito uno accettato all'apertura di una tappa, prima del suo primo
  pacchetto) il responsabile lo può correggere direttamente; l'intestazione
  dell'ADR riceve la riga **Corretto:** con la data e ciò che cambia.

## Comportamento

Si discute di codice e di progetto, non di persone. Toni aggressivi, insistenza
dopo un "no" motivato o il tentativo di aggirare la revisione portano prima a un
avviso, poi all'allontanamento dal progetto. La decisione spetta al responsabile.
