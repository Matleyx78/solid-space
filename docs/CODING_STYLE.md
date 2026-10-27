# Stile del codice

La formattazione è decisa da `clang-format`: non si discute. Lo strumento arriva con
la tappa 0.1 (pacchetto P3), con la stessa impostazione di Tecnigrafo. Questo
documento copre ciò che uno strumento non può decidere da solo; le decisioni di
fondo sono nell'[ADR 0003](adr/0003-convenzioni.md). Dove possibile le regole sono
quelle di Tecnigrafo, così chi lavora ai due progetti ritrova le stesse abitudini.

## Nomi

| Elemento | Stile | Esempio |
|---|---|---|
| Namespace | minuscolo, `solid_space::<componente>` | `solid_space::solver` |
| Tipi, classi, enum | `PascalCase` | `StableId` |
| Funzioni e metodi | `camelCase` | `orient2d()` |
| Variabili e parametri | `camelCase` | `maxIterations` |
| Membri privati | `m_` + `camelCase` | `m_lineage` |
| Costanti | `k` + `PascalCase` | `kDefaultTolerance` |
| File | `snake_case` | `stable_id.hpp` |
| Macro | evitarle; se proprio servono `SS_MAIUSCOLO` | `SS_VERSION` |

I nomi dicono che cosa fanno, non come: `orient2d`, non `computeDet3`.

## Regole

- Header con `#pragma once`, estensione `.hpp`; sorgenti `.cpp`.
- Ogni file nuovo inizia con l'intestazione SPDX dell'ADR 0003.
- Gli header pubblici stanno in
  `<componente>/include/solid_space/<componente>/` e includono solo la libreria
  standard e altri header pubblici del progetto, **mai** OCCT, libslvs o altre
  dipendenze esterne. Le strutture interne e gli header privati stanno in `src/`.
- Niente `using namespace` negli header.
- Niente `new`/`delete` espliciti: valori, `std::unique_ptr`, contenitori standard.
- Valori di ritorno che non vanno ignorati: `[[nodiscard]]`.
- Le forme del kernel e le istanze del solver sono maniglie opache, mai puntatori
  nudi; gli elementi si citano con `StableId`, mai con indirizzi.
- **Errori attesi come valori.** Un'operazione che può fallire restituisce un
  risultato con diagnosi (ADR 0003, punto 5). Le eccezioni delle librerie esterne
  si fermano dentro il componente e non attraversano mai le interfacce pubbliche.
- Le precondizioni si verificano con `assert` e si documentano nell'header.
- Nessun numero "magico": costanti con nome e un commento che ne spiega l'origine.
- Un commento spiega il *perché*, non il *cosa*. Per ogni `NOLINT` serve una riga
  che dice il motivo.
- Non si disattivano avvisi, controlli di analisi statica né test per far passare
  una compilazione.

## Geometria

- Lunghezze in `double`, nelle unità del modello; angoli in radianti.
- Mai confrontare due `double` con `==`, né con una soglia scritta a mano, per
  decidere un fatto geometrico: le decisioni discrete (orientamento, "dentro o
  fuori") passano dai predicati e dalla politica delle tolleranze di `robust`.
- Il sistema di coordinate del 3D si fissa nell'ADR della tappa che porta OCCT. Nel 2D le
  convenzioni sono quelle di Tecnigrafo (Y verso l'alto, angoli antiorari da +X).

## Test

- Un file di test per componente e per area, in `tests/<componente>/*_test.cpp`.
- Un test che prova un comportamento geometrico usa un caso dell'archivio
  `tests/corpus/` quando esiste, non valori ricavati eseguendo il codice
  (ADR 0004).
- Un test non dipende dall'ordine di esecuzione né da file fuori dal repository.
