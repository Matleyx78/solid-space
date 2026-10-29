Closes #<!-- numero della issue del pacchetto o del bug -->

**Tappa e pacchetto:** <!-- es. 0.2 / P3, 0.2 / R3 (riferimenti), oppure "correzione di un errore" -->

**Sblocca:** <!-- le issue di Tecnigrafo che questo pacchetto sblocca, scritte come Matleyx78/tecnigrafo#N (GitHub mostra il rimando in quelle issue), oppure "niente" (ADR 0007, punto 16) -->

## Cosa cambia

<!-- Due o tre frasi. -->

## Verifiche

- [ ] La issue collegata mi è stata assegnata
- [ ] La PR realizza un solo pacchetto, senza modifiche estranee
- [ ] Ho toccato solo i file elencati nel pacchetto (o ho spiegato qui perché no)
- [ ] Ogni condizione "Fatto quando" del pacchetto è verificata
- [ ] Rispetta gli ADR (nessuna nuova dipendenza, nessuna intestazione pubblica con tipi di OCCT o libslvs)
- [ ] Non cambia le intestazioni di un contratto accettato, oppure la scheda del pacchetto lo autorizza (etichetta `contratto`, ADR 0007, punto 15)
- [ ] Test richiesti presenti e funzionanti
- [ ] Modalità di controllo (ADR 0004), se esiste `tests/corpus/`:
  - lavoro: non ho toccato i casi che non sono del mio pacchetto
  - riferimenti: ho toccato solo `tests/corpus/`, i valori attesi non vengono da
    solid-space, ogni file preso dal web ha hash e licenza (link qui sotto)
- [ ] Ho scritto codice e documenti solo in solid-space (in Tecnigrafo, al massimo un commento richiesto)
- [ ] `clang-format` applicato, nessun avviso del compilatore né di `clang-tidy` (dalla consegna di P3)
- [ ] Commit in formato Conventional Commits (la firma DCO la aggiunge il responsabile allo squash).

## Uso dell'IA

- [ ] Nessuna IA usata
- [ ] IA usata: <!-- strumento --> — ho letto e capito ogni riga, ne rispondo io,
      e i commit riportano una riga di attribuzione
