# solid-space

> **Il kernel geometrico di [Tecnigrafo](https://github.com/Matleyx78/tecnigrafo):
> solver di vincoli, naming topologico stabile e modellazione solida robusta, in C++20.**

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Language: C++20](https://img.shields.io/badge/Language-C%2B%2B20-orange.svg)]()

**solid-space** è la libreria che darà a Tecnigrafo prima il solver dei vincoli 2D e
poi il modellatore solido 3D parametrico. Cresce a pezzi piccoli e verificabili,
nell'ordine in cui Tecnigrafo ne ha bisogno, e poggia su
[Open CASCADE Technology](https://dev.opencascade.org/) (OCCT) invece di rifarlo:
OCCT sta dietro una facciata nostra, e un suo modulo si sostituisce solo quando le
misure dimostrano che serve.

> Stato: **tappa 0.1, fondamenta, chiusa il 2026-10-27.** Scheletro CMake dei
> cinque componenti, CI sui tre sistemi e formato dei casi di verifica completati.
> La prova manuale del responsabile non è registrata nelle
> [note di rilascio](docs/rilasci/0.1.md). Nessun algoritmo geometrico ancora.
> **Tappa 0.2 in preparazione:** nasce dalle richieste #20–#23 della tappa 0.7
> di Tecnigrafo ([ADR 0007](docs/adr/0007-tappe-su-richiesta.md), corretto il
> 2026-10-29). Il piano è in [docs/ROADMAP.md](docs/ROADMAP.md).

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

| Componente | Contenuto | Dipende da |
|---|---|---|
| `robust` | predicati geometrici robusti e politica delle tolleranze | solo libreria standard |
| `solver` | interfaccia del solver di vincoli e motore (libslvs, poi altro) | `robust` |
| `naming` | identificativi stabili e grafo di genealogia | solo libreria standard |
| `kernel` | facciata su OCCT: forme, operazioni, STEP | `robust`, `naming`, OCCT |
| `features` | albero delle feature e rigenerazione, senza interfaccia grafica | `kernel`, `naming`, `solver` |

Ogni componente si compila da solo: il CAD 2D di Tecnigrafo userà `robust` e
`solver` senza avere OCCT. La mappa completa è in
[docs/ARCHITETTURA.md](docs/ARCHITETTURA.md).

## Cosa non è

Non è un tentativo di eguagliare Parasolid o ACIS: decenni di casi limite non si
rifanno con un piccolo gruppo. Le ambizioni del primo manifesto del progetto
(topologia cellulare, raccordi propri, tolleranze rigide) non sono state buttate:
stanno nell'appendice "Non ora" di [docs/VISIONE.md](docs/VISIONE.md), ciascuna con
il dato che la riporterebbe nel piano.

## Come si lavora

Con lo stesso metodo di Tecnigrafo, ma in un progetto che vive anche da solo: il
progetto avanza per **tappe**, una alla volta, ciascuna nata da una richiesta di
Tecnigrafo e divisa in **pacchetti di lavoro**;
le scelte di struttura si scrivono in **ADR** ([docs/adr/](docs/adr/)); un lavoro è
finito quando i test confermano i casi di verifica scritti prima. Il responsabile
del progetto ha l'ultima parola ([GOVERNANCE.md](GOVERNANCE.md)).

Come contribuire: [CONTRIBUTING.md](CONTRIBUTING.md). Persone e assistenti IA
lavorano con le stesse regole: per gli agenti sono in [AGENTS.md](AGENTS.md), lo
stile del codice in [docs/CODING_STYLE.md](docs/CODING_STYLE.md), il lavoro di più
agenti in parallelo in [docs/LAVORO-IN-PARALLELO.md](docs/LAVORO-IN-PARALLELO.md).

## Documentazione

- [docs/VISIONE.md](docs/VISIONE.md): scopo, principi e appendice "Non ora"
- [docs/ARCHITETTURA.md](docs/ARCHITETTURA.md): componenti, dipendenze e regole
- [docs/ROADMAP.md](docs/ROADMAP.md): tappe e decisioni del responsabile
- [docs/tappe/](docs/tappe/): schede delle tappe e modelli di piano e di pacchetto
- [docs/richieste/](docs/richieste/): modello della richiesta di Tecnigrafo a solid-space
- [docs/adr/](docs/adr/): decisioni architetturali
- [docs/LAVORO-IN-PARALLELO.md](docs/LAVORO-IN-PARALLELO.md): come lavorano più persone e agenti
- [docs/GUIDA-RESPONSABILE.md](docs/GUIDA-RESPONSABILE.md): guida passo per passo del responsabile
- [docs/AMBIENTE-SVILUPPO.md](docs/AMBIENTE-SVILUPPO.md): strumenti da installare
- [GOVERNANCE.md](GOVERNANCE.md) e [CONTRIBUTING.md](CONTRIBUTING.md): governo e contributi

## Licenza

GNU General Public License v3.0 o successiva (`GPL-3.0-or-later`).

## Componenti di terze parti

Sono sottomoduli in `third_party/`, fissati a una versione e non modificati:

- **Open CASCADE Technology**, LGPL 2.1 con eccezione: forme, operazioni e I/O STEP,
  solo nel componente `kernel`.
- **SolveSpace** (libslvs), GPLv3: primo motore del componente `solver`.
