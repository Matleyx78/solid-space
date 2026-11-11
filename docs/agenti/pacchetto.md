# Pacchetto di lavoro

Si legge insieme al nucleo, [AGENTS.md](../../AGENTS.md), quando la issue ha
l'etichetta `pacchetto` o il titolo `[X.Y-PN]`, quando ti chiedono «il prossimo
pacchetto», oppure quando tocchi file `.hpp` o `.cpp`. Le sezioni portano i titoli che avevano in AGENTS.md.

## Le etichette degli agenti

Viene da AGENTS.md, "Per prima cosa: prendi in carico", punto 2. Per ogni
sessione di Claude sono sue sia `agente:claude` sia `agente:claude-2`, che resta
solo sulle issue aperte prima della tappa che corrisponde alla 0.8 di Tecnigrafo.
Chi fa che cosa:
[LAVORO-IN-PARALLELO.md](../LAVORO-IN-PARALLELO.md), "Chi fa cosa".

## Prima di scrivere codice

Leggi, in quest'ordine:

1. [docs/ROADMAP.md](../ROADMAP.md), solo la tabella "Stato": quale tappa è
   **aperta**.
2. La **scheda** della tappa aperta, `docs/tappe/X.Y-nome.md`, e il pacchetto che
   ti è stato assegnato: file, interfacce, test e criterio di "fatto".
3. Gli ADR citati dalla scheda e [docs/ARCHITETTURA.md](../ARCHITETTURA.md).
4. [docs/CODING_STYLE.md](../CODING_STYLE.md).
5. Le intestazioni pubbliche del contratto che il pacchetto realizza o usa, e la
   lista di controllo di [docs/CONTRATTI.md](../CONTRATTI.md): vedi "Il
   contratto", in [contratto.md](contratto.md).

Lavora **su un solo pacchetto**. Se il compito non corrisponde a un pacchetto della
tappa aperta, non scrivere codice: serve prima una decisione del responsabile. Una
buona idea fuori dalla roadmap si propone, non si implementa.

Prima di cominciare controlla che non esista già un ramo o una pull request aperta
sullo stesso pacchetto: se c'è, non toccarlo e avvisa l'utente.

Ogni file di codice nuovo inizia con l'intestazione:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors
```

OCCT e libslvs si usano dai sottomoduli in `third_party/`, non da copie.

### Se ti chiedono di prendere il prossimo pacchetto

Scegli **una sola** issue, la prima nell'ordine del piano che:

1. è aperta e porta la **tua** etichetta `agente:…`;
2. non porta `AA-Presa in carico`;
3. ha chiuse tutte le issue della sua riga "Dipende da";
4. ha chiusa la sua issue di riferimenti (`[X.Y-RN]`), se esiste;
5. non ha aperta nessuna altra issue con l'etichetta `contratto` che nel titolo
   nomina un'intestazione che il pacchetto realizza o usa (la riga
   **Contratto:** della scheda; nelle schede che non l'hanno, le intestazioni
   pubbliche citate in **Da leggere**).

Per "il prossimo pacchetto di riferimenti" valgono le condizioni 1–3 e 5, sulle
issue con l'etichetta `riferimenti` ([riferimenti.md](riferimenti.md)). Se nessuna
issue va bene, non iniziare: scrivi all'utente quali issue sono tue e cosa le
blocca. Il pacchetto successivo si prende in una nuova sessione, dopo il merge
(le eccezioni sono in [lotti.md](lotti.md)).

## Robustezza e verifica

Un pacchetto che cambia il comportamento geometrico è finito quando i test
confermano casi di verifica scritti **prima** da un agente o da una persona
**diversi** da chi scrive il codice ([ADR 0004](../adr/0004-robustezza-e-verifica.md)).
Chi scrive i casi segue [riferimenti.md](riferimenti.md).

### In un pacchetto di lavoro

- Non modificare i casi in `tests/corpus/` che non sono del tuo pacchetto.
- Se un caso ti sembra sbagliato, non fermarti al primo: guarda **tutti** i
  casi del pacchetto, senza cambiare nulla, e poi **fermati del tutto una volta
  sola**, anche se il resto è pronto:
  1. non adattare il codice al riferimento che ritieni sbagliato né il
     riferimento al codice; il caso resta com'è;
  2. non aprire la pull request e non avviare la CI;
  3. scrivi nella issue (e all'utente), **in un solo commento** per tutti i
     casi che ti sembrano sbagliati, il nome di ciascuno, il valore atteso,
     quello che dà il programma e perché il riferimento ti sembra sbagliato;
  4. se hai lavoro non pubblicato, pubblica il ramo **senza** pull request e
     scrivi nel commento il nome del ramo;
  5. togli `AA-Presa in carico` e termina la sessione.

  Il caso lo corregge un pacchetto di riferimenti; poi il pacchetto si
  riprende. In un lotto passa al pacchetto successivo.
- Un'operazione che può fallire restituisce un risultato con diagnosi; un ripiego
  non è mai silenzioso.

## Regole del codice

Vengono da AGENTS.md, "Regole che non si possono violare", e valgono come quelle:

- **Nessuna intestazione pubblica include OCCT, libslvs, Eigen o altre dipendenze
  esterne** (ADR 0002). Le forme e le strutture del solver sono maniglie opache.
- **Le dipendenze tra componenti vanno in un verso solo**: `features` →
  `kernel`, `naming`, `solver`; `kernel` → `robust`, `naming`, `geom2d`;
  `solver` → `robust`, `geom2d`; `geom2d` → `robust`. `robust` e `naming` non
  dipendono da nulla. Un componente non include uno che sta sopra di lui
  ([ADR 0008](../adr/0008-componente-geom2d.md)).
- Gli elementi si citano con identificativi stabili (`StableId`), mai con
  puntatori o indirizzi, e mai con tipi del nucleo.
- Nessuna eccezione attraversa un'interfaccia pubblica: gli errori sono valori con
  diagnosi (ADR 0003). Unica eccezione ammessa: `std::bad_alloc` quando la
  memoria è esaurita, che non si intercetta (ADR 0003, punto 5).
