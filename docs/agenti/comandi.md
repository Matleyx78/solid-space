# Comandi, build e CI

Si legge insieme al nucleo, [AGENTS.md](../../AGENTS.md), quando apri o aggiorni
una pull request, quando lanci uno script di `scripts/`, oppure quando il lavoro
tocca `scripts/`, `CMakeLists.txt`, `CMakePresets.json`, `cmake/` o `.github/`.
Le sezioni portano i titoli che avevano in AGENTS.md.

## Comandi

Un solo comando esegue formato (clang-format 18), configurazione, compilazione e
test, fermandosi al primo errore e mostrando la coda del log:

```sh
scripts/controlla.sh
scripts/controlla.sh --tidy              # anche clang-tidy 18 sui file toccati
scripts/controlla.sh --solo formato      # oppure configura, compila, test, tidy
SS_BUILD_SUFFIX=-codex scripts/controlla.sh --tidy
SS_PRESET=asan scripts/controlla.sh      # AddressSanitizer e UBSan
```

Servono gli strumenti di [AMBIENTE-SVILUPPO.md](../AMBIENTE-SVILUPPO.md).
`SS_PRESET` vale `release` se omesso; `SS_BUILD_SUFFIX` separa le cartelle di build,
così più agenti sullo stesso PC non si bloccano a vicenda: usa la tua (`-claude`
o `-codex`; una seconda sessione di Claude sullo stesso PC usa `-claude-2`).
`--solo` presuppone che i passi precedenti siano già stati eseguiti.
Per l'analisi dei file modificati il riferimento è `origin/main`; si può scegliere
un altro commit con `SS_BASE_REF`. Se cambia la configurazione dell'analisi, si
controllano tutti i sorgenti. La CI usa lo stesso comando per build e test.

Una pull request che tocca solo documenti, commenti o casi di riferimento **non
si compila in locale**: la verifica la fa la CI (decisione del 2026-11-03,
[DECISIONI.md](../DECISIONI.md)).

Se nel tuo ambiente mancano gli strumenti di build (compilatori, CMake, Ninja o
altri pacchetti di sistema), **non installarli**: un'installazione dura a lungo, può
bloccarsi e far perdere il lavoro non ancora pubblicato. Esegui i passi possibili,
pubblica il ramo e scrivi nella pull request quali passi non hai eseguito. In
particolare **non compilare OCCT** a mano: è un lavoro da decine di minuti che la
CI fa con una cache.

## Aspettare la CI

Viene da AGENTS.md, "Quando fermarsi", che resta nel nucleo. La condizione dei
controlli verdi vale da quando esiste la CI (pacchetto P3 della tappa 0.1).

Apri la pull request e aspetta la CI con **un solo comando che blocca**, non con
controlli ripetuti (ogni controllo consuma token). Mentre aspetti non fare
domande e non cominciare altro.

Nel riepilogo e nella pull request scrivi anche ciò che non hai potuto
verificare, per esempio Windows e macOS.

Aspetta la CI e leggine gli errori con un solo comando per operazione (solo API
REST; servono `jq` e `gh` oppure `curl`):

```sh
scripts/ci-attendi.sh             # CI del commit corrente, già pubblicato
scripts/ci-attendi.sh <sha>       # oppure un commit preciso
scripts/ci-attendi.sh <numero>    # oppure l'ultimo commit di una pull request
scripts/ci-attendi.sh <run-id>    # oppure una run precisa
scripts/ci-errori.sh <run-id>     # job falliti, passi e righe di errore
```

Un numero fino a sei cifre è una pull request, uno più lungo l'id di una run;
`ci-errori.sh` accetta gli stessi argomenti.

Il repository pubblico si legge anche senza token con `curl`; `GITHUB_TOKEN`
aumenta il limite delle richieste anonime. `SS_REPO` cambia il repository.
`ci-attendi.sh` restituisce 0 con tutto verde, 1 se un controllo fallisce,
2 se l'API non risponde, 3 se la CI è ancora in corso, 4 se è annullata,
5 dopo dieci attese consecutive scadute sulla stessa run, 6 se la run aspetta
un'approvazione (`action_required`) da più di 20 minuti: lo script ha già
scritto una volta «CI da approvare» nella pull request. Con 3 rilancia lo
stesso comando; con 1 leggi subito gli errori, senza aspettare gli altri job.
Con 2, 4 o 5 riferisci il problema nella pull request e termina come dice
AGENTS.md, "Quando fermarsi"; con 6 lo stesso, senza `decisione`
([catena.md](catena.md), "Fermate senza decisione").
`SS_ATTESA_MAX`, `SS_ATTESA_PAUSA`, `SS_ATTESA_RILANCI` e
`SS_ATTESA_APPROVAZIONE` regolano durata, intervallo, numero massimo di attese e
attesa di un'approvazione (default: 540 secondi, 90 secondi, dieci, 1200
secondi).

## Autoprove degli script

Un'autoprova scritta in Python o in shell (sotto `scripts/` o `tests/corpus/`)
**non si ferma al primo errore**: raccoglie tutti i controlli falliti, alla
fine li stampa tutti, con nome del controllo, valore atteso e valore ottenuto,
e solo allora esce con errore. I test C++ lo fanno già
([DECISIONI.md](../DECISIONI.md), 2026-11-21).

## Autoprove degli script

Un'autoprova scritta in Python o in shell (sotto `scripts/` o `tests/corpus/`)
**non si ferma al primo errore**: raccoglie tutti i controlli falliti, alla
fine li stampa tutti, con nome del controllo, valore atteso e valore ottenuto,
e solo allora esce con errore. I test C++ lo fanno già
([DECISIONI.md](../DECISIONI.md), 2026-11-21).

## Gli ambienti della catena

Nella catena degli agenti ([catena.md](catena.md)) Claude lavora in GitHub
Actions (`.github/workflows/claude.yml`, con `SS_BUILD_SUFFIX=-claude`) e Codex
nel suo ambiente sul web. **Che cosa c'è in questi ambienti lo decide il
responsabile**: gli script di preparazione degli ambienti di Codex, i passi del
workflow `claude.yml` e l'elenco dei comandi che Claude può lanciare
([ADR 0018](../adr/0018-catena-degli-agenti.md)). Un agente non li cambia e non
ne propone di nuovi nella pull request del suo lavoro: se gli manca qualcosa,
vale la regola sopra (niente installazioni, solo i passi possibili, il resto
alla CI) e lo scrive nella pull request; una proposta di cambio va nella issue
«Proposte di metodo» della tappa.

In GitHub Actions `gh` usa il token personale della catena (secret
`RELAY_PAT`), che non legge le annotazioni degli errori: `scripts/ci-errori.sh`
le legge con il token del job (`GH_TOKEN_LETTURA`), e altrove senza token.
