# Architettura in breve

La mappa per orientarsi nel codice. Le motivazioni sono negli ADR, elencati in
fondo.

## Componenti

```text
features -> kernel, naming, solver
kernel   -> robust, naming, OCCT (privato)
solver   -> robust, libslvs (privato)
naming   -> (nessuna dipendenza)
robust   -> (nessuna dipendenza)
```

Le dipendenze vanno in un solo verso: nessun componente conosce chi lo usa. Le
operazioni di `kernel` passano a `naming` la genealogia che emettono.

| Componente | Namespace | Contiene | Può usare |
|---|---|---|---|
| `robust` | `solid_space::robust` | predicati orient/incircle (2D, poi 3D), politica delle tolleranze | solo libreria standard |
| `naming` | `solid_space::naming` | `StableId`, grafo di genealogia, serializzazione | solo libreria standard |
| `solver` | `solid_space::solver` | interfaccia del solver, motore su libslvs, diagnosi dei conflitti | `robust`, libslvs (privato) |
| `kernel` | `solid_space::kernel` | forme opache, primitive, estrusione e rivoluzione, booleane, raccordi, controllo di validità, strategie di ripiego, STEP | `robust`, `naming`, OCCT (privato) |
| `features` | `solid_space::features` | grafo delle feature, rigenerazione, risoluzione dei riferimenti | `kernel`, `naming`, `solver` |

Ogni componente è un target CMake con alias `solid_space::<nome>` e si compila da
solo. Il CAD 2D di Tecnigrafo userà `robust` e `solver`, senza OCCT; l'opzione che
spegne `kernel` e `features` si definirà nella tappa 0.1.

## Regole che valgono ovunque

1. **Le interfacce pubbliche non mostrano dipendenze esterne.** Nessuna
   intestazione pubblica include OCCT, libslvs, Eigen o altro: le forme sono
   maniglie opache, le strutture interne stanno dietro il file `.cpp`.
2. **Nessun puntatore di memoria come riferimento.** Schizzi, vincoli e feature
   citano `StableId`, mai indirizzi né tipi del nucleo.
3. **Gli errori sono valori.** Nessuna eccezione attraversa l'interfaccia
   pubblica: le operazioni restituiscono un risultato con diagnosi (ADR 0003).
4. **Risultati deterministici.** Stessi ingressi, stessi identificativi e stessi
   risultati sui tre sistemi operativi (ADR 0004).
5. **Dipendenze verso il basso.** Un componente non può includere uno che sta
   sopra di lui nel grafo.

## Naming topologico

Il componente `naming` non conosce OCCT. Le operazioni del `kernel` gli passano che
cosa è stato generato, modificato o cancellato (gli algoritmi di costruzione di
OCCT espongono queste informazioni), e `naming` ne ricava gli identificativi e
tiene il grafo di genealogia. La funzione degli identificativi e il formato di
salvataggio si fissano con l'ADR della tappa 0.4.

## Dove si trova cosa

```text
<componente>/CMakeLists.txt                    target e dipendenze
<componente>/include/solid_space/<componente>/ interfaccia pubblica
<componente>/src/                              implementazione (anche intestazioni private)
tests/<componente>/                            test del componente
tests/corpus/                                  archivio dei casi di verifica (ADR 0004)
third_party/                                   OCCT e SolveSpace, sottomoduli fissati e non modificati
docs/                                          visione, architettura, roadmap, tappe, ADR
```

Per ora esiste solo `kernel/` con le cartelle vuote; le altre nascono con la loro
tappa.

## Dipendenze esterne

- **OCCT** si cerca prima come pacchetto di sistema (`find_package`); in
  mancanza si usa il sottomodulo `third_party/occt`, a una versione fissata. Lo
  collega solo `kernel`, in modo privato.
- **libslvs** viene dal sottomodulo `third_party/solvespace`, non modificato. Lo
  collega solo `solver`, in modo privato. Le correzioni vanno al progetto a monte
  o, se servono, in un ADR che spiega perché.
- Nessuna nuova dipendenza senza un ADR.

## Decisioni architetturali

| ADR | Titolo | Stato |
|---|---|---|
| [0000](adr/0000-modello.md) | Modello di ADR | Modello |
| [0001](adr/0001-scopo-e-confini.md) | Scopo e confini | Proposto |
| [0002](adr/0002-componenti-e-dipendenze.md) | Componenti e dipendenze | Proposto |
| [0003](adr/0003-convenzioni.md) | Convenzioni di linguaggio, errori e stile | Proposto |
| [0004](adr/0004-robustezza-e-verifica.md) | Robustezza e verifica | Proposto |
