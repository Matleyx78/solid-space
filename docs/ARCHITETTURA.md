# Architettura in breve

La mappa per orientarsi nel codice. Le motivazioni sono negli ADR, elencati in
fondo.

## Componenti

```text
features -> kernel, naming, solver
kernel   -> robust, naming, geom2d (quando serve), OCCT (privato)
solver   -> robust, geom2d (quando serve), libslvs (privato)
geom2d   -> robust
naming   -> (nessuna dipendenza)
robust   -> (nessuna dipendenza)
```

Le dipendenze vanno in un solo verso: nessun componente conosce chi lo usa. Le
operazioni di `kernel` passano a `naming` la genealogia che emettono.

| Componente | Namespace | Contiene | Può usare |
|---|---|---|---|
| `robust` | `solid_space::robust` | tipi di base (`Point2`, `Result`), predicati esatti (2D, poi 3D), trigonometria deterministica, politica delle tolleranze | solo libreria standard |
| `geom2d` | `solid_space::geom2d` | curve del piano (segmento, arco, cerchio, segmento di polilinea, polilinea), intersezioni e lato, tangenze e raccordi, curva parallela, regola delle tolleranze sulle curve | `robust` |
| `naming` | `solid_space::naming` | `StableId`, grafo di genealogia, serializzazione | solo libreria standard |
| `solver` | `solid_space::solver` | interfaccia del solver, motore su libslvs, diagnosi dei conflitti | `robust`, `geom2d` (quando serve), libslvs (privato) |
| `kernel` | `solid_space::kernel` | forme opache, primitive, estrusione e rivoluzione, booleane, raccordi, controllo di validità, strategie di ripiego, STEP | `robust`, `naming`, `geom2d` (quando serve), OCCT (privato) |
| `features` | `solid_space::features` | grafo delle feature, rigenerazione, risoluzione dei riferimenti | `kernel`, `naming`, `solver` |

Ogni componente è un target CMake con alias `solid_space::<nome>` e si compila da
solo. Il CAD 2D di Tecnigrafo userà `robust`, `geom2d` e `solver`, senza OCCT: l'opzione
CMake `SOLID_SPACE_BUILD_KERNEL=OFF` spegne `kernel` e `features`.

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
6. **Decisioni esatte, costruzioni con errore dichiarato.** In `robust` e
   `geom2d` ogni decisione è esatta sui `double` ricevuti, ogni costruzione
   dichiara il suo errore, e le tolleranze si applicano solo con le funzioni
   che le nominano. Il calcolo in virgola mobile sta nei `.cpp`, senza
   contrazione FMA e senza la trigonometria della libreria standard
   ([ADR 0008](adr/0008-componente-geom2d.md), punti 5 e 6).

## Naming topologico

Il componente `naming` non conosce OCCT. Le operazioni del `kernel` gli passano che
cosa è stato generato, modificato o cancellato (gli algoritmi di costruzione di
OCCT espongono queste informazioni), e `naming` ne ricava gli identificativi e
tiene il grafo di genealogia. La funzione degli identificativi e il formato di
salvataggio si fissano con l'ADR della tappa che porta il naming.

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

Ogni componente ha la sua cartella con `CMakeLists.txt` e `src/`; le intestazioni
pubbliche nascono con la tappa che le porta. `robust` e `geom2d` hanno già le
intestazioni del contratto con Tecnigrafo, in bozza e non ancora realizzate
([ADR 0007](adr/0007-tappe-su-richiesta.md), punto 15).

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
| [0001](adr/0001-scopo-e-confini.md) | Scopo e confini | Accettato |
| [0002](adr/0002-componenti-e-dipendenze.md) | Componenti e dipendenze | Accettato |
| [0003](adr/0003-convenzioni.md) | Convenzioni di linguaggio, errori e stile | Accettato |
| [0004](adr/0004-robustezza-e-verifica.md) | Robustezza e verifica | Accettato |
| [0005](adr/0005-formato-dei-casi-di-verifica.md) | Formato dei casi di verifica e strumenti di controllo | Accettato |
| [0006](adr/0006-build-ci-e-sottomoduli.md) | Build, CI e sottomoduli | Accettato |
| [0007](adr/0007-tappe-su-richiesta.md) | Le tappe nascono dalle richieste di Tecnigrafo | Accettato |
| [0008](adr/0008-componente-geom2d.md) | Il componente `geom2d` | Accettato |
| [0009](adr/0009-regola-delle-tolleranze-del-piano.md) | Una regola unica per coincidenze e tolleranze del piano (richiesta #23, esito 3) | Accettato |
| [0010](adr/0010-posizione-e-intersezioni-nel-piano.md) | Posizione reciproca e intersezioni di linee, archi e cerchi del piano (richiesta #20, esito 3) | Accettato |
