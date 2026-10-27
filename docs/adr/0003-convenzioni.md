# ADR 0003: Convenzioni di linguaggio, errori e stile

- **Stato:** Proposto
- **Data:** 2026-10-27

## Contesto

solid-space e Tecnigrafo sono sviluppati dalle stesse persone e dagli stessi
agenti IA. Conviene che strumenti, stile e regole siano gli stessi, così chi passa
da un progetto all'altro ritrova gli stessi comandi.

## Decisione

1. **Linguaggio e strumenti.** C++20, CMake ≥ 3.24, Ninja, GoogleTest,
   clang-format e clang-tidy 18. Avvisi del compilatore trattati come errori.
   Stessa impostazione di Tecnigrafo per formato, avvisi e preset.
2. **Licenza.** `GPL-3.0-or-later`. Ogni file nuovo inizia con
   ```cpp
   // SPDX-License-Identifier: GPL-3.0-or-later
   // Copyright (C) 2026 The solid-space authors
   ```
   Dipendenze ammesse solo con licenze compatibili con la GPLv3 (LGPL, GPL, MIT,
   BSD, Apache 2.0, MPL 2.0), e sempre con un ADR.
3. **Lingua.** Codice, commenti e messaggi di commit in **inglese**. Documenti,
   ADR e schede in **italiano**.
4. **Interfacce pubbliche.** `<componente>/include/solid_space/<componente>/*.hpp`,
   namespace `solid_space::<componente>`. Nessun tipo di dipendenza esterna
   nelle intestazioni (ADR 0002).
5. **Errori come valori.** Nessuna eccezione attraversa l'interfaccia pubblica.
   Un'operazione che può fallire restituisce un risultato che porta o il valore o
   una **diagnosi**: un codice stabile leggibile da una macchina, un messaggio
   per chi legge e il luogo del problema (identificativi stabili degli elementi
   coinvolti). Le eccezioni delle librerie esterne si fermano dentro il
   componente. Chi chiama l'interfaccia in modo che viola una precondizione
   documentata ottiene un'asserzione in debug, non un risultato.
6. **Nessun puntatore come riferimento.** Gli elementi si citano con identificativi
   stabili (ADR 0001 e VISIONE.md), mai con indirizzi di memoria.
7. **Numeri.** Lunghezze in `double` nelle unità del modello, senza conversioni
   interne; angoli in radianti. Sistema di coordinate e unità del 3D si fissano
   nell'ADR della tappa 0.5.
8. **Commit e pull request.** Formato Conventional Commits
   (`feat(solver): ...`). Una pull request per pacchetto. Il responsabile aggiunge
   la firma di origine (`Signed-off-by`) a ciò che unisce; gli agenti non la
   aggiungono.
9. **Codice altrui.** Non si copia codice da altri progetti salvo richiesta
   esplicita del responsabile, con fonte, versione e licenza registrate. Usare
   una libreria come sottomodulo o come dipendenza non è copiare.

## Alternative considerate

- **Eccezioni nell'interfaccia pubblica**: attraversano male i confini tra
  componenti e librerie e rendono i fallimenti non verificabili dai test.
- **Stile diverso da Tecnigrafo**: nessun vantaggio, solo attrito per chi lavora a
  entrambi.

## Conseguenze

- I fallimenti sono dati che si possono confrontare nei casi di verifica
  (ADR 0004).
- La regola sul codice altrui si applica anche agli agenti IA: l'uso di libslvs e
  OCCT passa dai sottomoduli, non da copie.
