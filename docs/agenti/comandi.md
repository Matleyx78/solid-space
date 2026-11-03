# Comandi, build e CI

Si legge insieme al nucleo, [AGENTS.md](../../AGENTS.md), quando apri o aggiorni
una pull request, quando esegui `scripts/controlla.sh` o `scripts/ci-*.sh`,
oppure quando il lavoro tocca `CMakeLists.txt`, `CMakePresets.json`, `cmake/`,
`scripts/`, `.github/` o `third_party/`. Le sezioni portano i titoli che avevano
in AGENTS.md.

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
così più agenti sullo stesso PC non si bloccano a vicenda: usa la tua (`-claude`,
`-codex`, `-gemini`, `-gemini-argon`; una seconda sessione di Claude sullo stesso
PC usa `-claude-2`).
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

Aspetta la CI e leggine gli errori con un solo comando per operazione (solo API
REST; servono `jq` e `gh` oppure `curl`):

```sh
scripts/ci-attendi.sh             # CI del commit corrente, già pubblicato
scripts/ci-attendi.sh <sha>       # oppure un commit preciso
scripts/ci-attendi.sh <run-id>    # oppure una run precisa
scripts/ci-errori.sh <run-id>     # job falliti, passi e righe di errore
```

Il repository pubblico si legge anche senza token con `curl`; `GITHUB_TOKEN`
aumenta il limite delle richieste anonime. `SS_REPO` cambia il repository.
`ci-attendi.sh` restituisce 0 con tutto verde, 1 se un controllo fallisce,
2 se l'API non risponde, 3 se la CI è ancora in corso, 4 se è annullata,
5 dopo dieci attese consecutive scadute sulla stessa run. Con 3 rilancia lo
stesso comando; con 1 leggi subito gli errori, senza aspettare gli altri job.
Con 2, 4 o 5 riferisci il problema nella pull request e termina come dice
AGENTS.md, "Quando fermarsi".
`SS_ATTESA_MAX`, `SS_ATTESA_PAUSA` e `SS_ATTESA_RILANCI` regolano durata,
intervallo e numero massimo di attese (default: 540 secondi, 90 secondi, dieci).
