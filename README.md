# solid-space

> **Kernel geometrico C++20 per CAD: geometria 2D robusta, solver di vincoli,
> naming topologico stabile e modellazione solida.**
> Nasce per [Tecnigrafo](https://github.com/Matleyx78/tecnigrafo), ma è un progetto
> autonomo che si può usare in altri programmi.

[![CI](https://github.com/Matleyx78/solid-space/actions/workflows/ci.yml/badge.svg)](https://github.com/Matleyx78/solid-space/actions/workflows/ci.yml)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Language: C++20](https://img.shields.io/badge/Language-C%2B%2B20-orange.svg)]()

> **In English.** solid-space is a C++20 library that gives
> [Tecnigrafo](https://github.com/Matleyx78/tecnigrafo), a free 2D/3D CAD, its
> geometric foundations: robust 2D geometry, a constraint solver, stable
> topological naming and solid modeling on top of Open CASCADE. It grows in
> small, verifiable steps. Today only `robust` and `geom2d` contain code, and
> their contracts may still change; the other components are skeletons. It has
> no dependencies beyond the standard library for those two, builds with CMake
> 3.24 or newer, and is licensed under GPL-3.0-or-later. Documentation is in
> Italian; code, comments and commit messages are in English.

## In breve

**solid-space** è una libreria C++20 che dà a Tecnigrafo, un CAD libero 2D e 3D, le
sue basi geometriche: costruzioni e decisioni **robuste** sul piano, un **solver**
di vincoli, **identificativi stabili** per la topologia e **operazioni solide**
costruite su [Open CASCADE Technology](https://dev.opencascade.org/) (OCCT) invece
di rifarlo. Cresce a pezzi piccoli e verificabili, nell'ordine in cui Tecnigrafo ne
ha bisogno.

È pensata per chi scrive software di geometria o CAD in C++ e vuole un kernel con
regole dichiarate: ogni operazione pubblica ha un **contratto scritto** nelle
intestazioni (che cosa garantisce, che cosa risponde con dati fuori dominio) e un
archivio di casi di verifica, non un insieme di funzioni che "di solito
funzionano".

**Stato.** Oggi sono disponibili, in sviluppo, i componenti `robust` e `geom2d`: si
usano già, senza OCCT né Qt, ma il loro contratto può ancora cambiare. Solver,
naming, kernel su OCCT e feature non sono ancora cominciati (c'è solo lo
scheletro). Dettagli nella tabella qui sotto e in
[docs/ROADMAP.md](docs/ROADMAP.md).

## Cosa vuole dare

1. **Solver variazionale 2D e 3D** dietro un'interfaccia nostra: entità, vincoli e
   chiavi stabili in ingresso, diagnosi dei conflitti in uscita. Il motore numerico
   resta sostituibile.
2. **Naming topologico stabile**: ogni operazione emette identificativi derivati
   dalla sua genealogia, così schizzi, vincoli e feature non puntano mai a
   puntatori di memoria e sopravvivono alla rigenerazione dell'albero.
3. **Operazioni solide robuste**: booleane, raccordi, STEP. La robustezza viene da
   un metodo (controllo di validità, strategie alternative, diagnosi chiare, un
   archivio di casi difficili che cresce) prima che da riscritture.

## Componenti

| Componente | Contenuto | Dipende da | Stato |
|---|---|---|---|
| `robust` | predicati geometrici robusti, trigonometria deterministica e politica delle tolleranze | solo libreria standard | in sviluppo (tappa 0.2) |
| `geom2d` | curve del piano: intersezioni, lato, tangenze e raccordi, curva parallela | `robust` | in sviluppo (tappa 0.2) |
| `solver` | interfaccia del solver di vincoli e motore (libslvs, poi altro) | `robust`, `geom2d` (quando serve) | non ancora cominciato |
| `naming` | identificativi stabili e grafo di genealogia | solo libreria standard | non ancora cominciato |
| `kernel` | facciata su OCCT: forme, operazioni, STEP | `robust`, `naming`, `geom2d` (quando serve), OCCT | non ancora cominciato |
| `features` | albero delle feature e rigenerazione, senza interfaccia grafica | `kernel`, `naming`, `solver` | non ancora cominciato |

Ogni componente si compila da solo: il CAD 2D di Tecnigrafo usa `robust` e
`geom2d` senza avere OCCT. La mappa completa è in
[docs/ARCHITETTURA.md](docs/ARCHITETTURA.md).

## Usarlo nel tuo progetto

Servono un compilatore C++20 a 64 bit (GCC, Clang o MSVC) e CMake 3.24 o più
recente. `robust` e `geom2d` non hanno altre dipendenze: non servono OCCT né Qt.
Non abilitare `-ffast-math` né `/fp:fast` per i target di solid-space: la
configurazione controlla questi requisiti.

Non esiste ancora una versione con gli algoritmi: fissa il tuo progetto a un
commit preciso di `main`. Ogni pacchetto unito lascia `main` utilizzabile e con la
CI verde. Il modo più semplice è un sottomodulo, come fa Tecnigrafo:

```sh
git submodule add https://github.com/Matleyx78/solid-space third_party/solid-space
```

Non usare `--recursive`: i sottomoduli di solid-space (OCCT e SolveSpace) non
servono ai componenti già disponibili.

```cmake
set(SOLID_SPACE_BUILD_KERNEL OFF)   # kernel e features sono i soli a richiedere OCCT
add_subdirectory(third_party/solid-space)

add_executable(esempio main.cpp)
target_link_libraries(esempio PRIVATE solid_space::geom2d)
```

Un primo esempio: dove un segmento taglia un cerchio.

```cpp
#include <iostream>

#include <solid_space/geom2d/curves.hpp>
#include <solid_space/geom2d/intersection.hpp>

int main() {
    using namespace solid_space::geom2d;

    const Segment line{{-5.0, 0.0}, {5.0, 0.0}};
    const Circle circle{{0.0, 0.0}, 3.0};

    const auto result = intersect(line, Extension::none, circle, Extension::none);
    if (!result.hasValue()) {
        std::cerr << "invalid input\n";   // result.diagnosis() dice quale e perché
        return 1;
    }
    for (const auto& hit : result.value().points) {
        std::cout << "(" << hit.point.x << ", " << hit.point.y << ")\n";
    }
}
```

Stampa `(-3, 0)` e `(3, 0)`. Le operazioni che possono fallire restituiscono un
`Result`: o il valore o una diagnosi, mai un'eccezione. Che cosa garantisce ogni
funzione sta nei commenti delle intestazioni in `geom2d/include/solid_space/` e
in [docs/CONTRATTI.md](docs/CONTRATTI.md).

## Compilare e provare i test

Su Debian o Ubuntu (altri sistemi e dettagli in
[docs/AMBIENTE-SVILUPPO.md](docs/AMBIENTE-SVILUPPO.md)):

```sh
sudo apt install build-essential cmake ninja-build libgtest-dev python3
cmake --preset release
cmake --build --preset release
ctest --preset release
```

I preset sono `debug`, `release` e `asan` (AddressSanitizer e UBSan). Se la
GoogleTest del sistema è troppo vecchia, CMake scarica da sola la versione giusta
alla prima configurazione. La CI prova Linux, Windows e macOS.

## Cosa non è

Non è un tentativo di eguagliare Parasolid o ACIS: decenni di casi limite non si
rifanno con un piccolo gruppo. Le ambizioni del primo manifesto del progetto
(topologia cellulare, raccordi propri, tolleranze rigide) non sono state buttate:
stanno nell'appendice "Non ora" di [docs/VISIONE.md](docs/VISIONE.md), ciascuna con
il dato che la riporterebbe nel piano.

## Come si lavora

Con lo stesso metodo di Tecnigrafo, ma in un progetto che vive anche da solo: il
progetto avanza per **tappe**, una alla volta, ciascuna nata da una richiesta di
Tecnigrafo e divisa in **pacchetti di lavoro**; le scelte di struttura si scrivono
in **ADR** ([docs/adr/](docs/adr/)); un lavoro è finito quando i test confermano i
casi di verifica scritti prima. Il responsabile del progetto ha l'ultima parola
([GOVERNANCE.md](GOVERNANCE.md)).

Tappa 0.1 (fondamenta) chiusa il 2026-10-27, con le
[note di rilascio](docs/rilasci/0.1.md); la prova manuale su Windows nativo resta
da eseguire. **Tappa 0.2 aperta il 2026-10-30**: nasce dalle richieste #20–#23 della
tappa 0.7 di Tecnigrafo ([ADR 0007](docs/adr/0007-tappe-su-richiesta.md)), il piano
è in [docs/ROADMAP.md](docs/ROADMAP.md).

Come contribuire: [CONTRIBUTING.md](CONTRIBUTING.md). Persone e assistenti IA
lavorano con le stesse regole: per gli agenti sono in [AGENTS.md](AGENTS.md), lo
stile del codice in [docs/CODING_STYLE.md](docs/CODING_STYLE.md), il lavoro di più
agenti in parallelo in [docs/LAVORO-IN-PARALLELO.md](docs/LAVORO-IN-PARALLELO.md).

## Documentazione

Per capire e usare il progetto:

- [docs/VISIONE.md](docs/VISIONE.md): scopo, principi e appendice "Non ora"
- [docs/ARCHITETTURA.md](docs/ARCHITETTURA.md): componenti, dipendenze e regole
- [docs/CONTRATTI.md](docs/CONTRATTI.md): che cosa garantiscono le intestazioni pubbliche
- [docs/adr/](docs/adr/): decisioni architetturali
- [docs/ROADMAP.md](docs/ROADMAP.md): tappe e loro regole

Per lavorarci:

- [docs/AMBIENTE-SVILUPPO.md](docs/AMBIENTE-SVILUPPO.md): strumenti da installare
- [GOVERNANCE.md](GOVERNANCE.md) e [CONTRIBUTING.md](CONTRIBUTING.md): governo e contributi
- [docs/DECISIONI.md](docs/DECISIONI.md): decisioni del responsabile
- [docs/tappe/](docs/tappe/): schede delle tappe e modelli di piano e di pacchetto
- [docs/richieste/](docs/richieste/): modello della richiesta di Tecnigrafo a solid-space
- [docs/LAVORO-IN-PARALLELO.md](docs/LAVORO-IN-PARALLELO.md): come lavorano più persone e agenti
- [docs/GUIDA-RESPONSABILE.md](docs/GUIDA-RESPONSABILE.md): guida passo per passo del responsabile

## Licenza

GNU General Public License v3.0 o successiva (`GPL-3.0-or-later`). Un programma che
incorpora solid-space e viene distribuito ricade negli obblighi della GPLv3: leggi
la licenza prima di adottarlo.

## Componenti di terze parti

Sono sottomoduli in `third_party/`, fissati a una versione e non modificati:

- **Open CASCADE Technology**, LGPL 2.1 con eccezione: forme, operazioni e I/O STEP,
  solo nel componente `kernel`.
- **SolveSpace** (libslvs), GPLv3: primo motore del componente `solver`.
