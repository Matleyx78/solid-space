# Ambiente di sviluppo

Procedura per Debian 13 (trixie). Su Windows 11 la stessa procedura funziona dentro
WSL (Debian dentro Windows): tieni il progetto **dentro Debian**
(`~/progetti/solid-space`), non sotto `/mnt/c`, e usa il `git` di Debian. Una
build fatta in WSL è una build **Linux**: per Windows e macOS fa fede la CI.

Questa guida copre gli strumenti. I comandi per compilare e provare il codice
arrivano con i pacchetti P2 e P3 della [tappa 0.1](tappe/0.1-fondamenta.md) e
saranno scritti in [AGENTS.md](../AGENTS.md), sezione "Comandi".

## 1. Pacchetti di sistema

```sh
sudo apt update
sudo apt install build-essential cmake ninja-build git gdb libgtest-dev python3
```

`python3` serve solo agli strumenti di sviluppo (il controllo dei casi di verifica,
[ADR 0005](adr/0005-formato-dei-casi-di-verifica.md)), non alla libreria.

Non servono Qt né altre librerie grafiche: solid-space non ha interfaccia. OCCT
non si installa: dalla tappa che lo porta c'è il sottomodulo `third_party/occt`, o un
pacchetto di sistema se c'è ([ADR 0002](adr/0002-componenti-e-dipendenze.md)).

Se la GoogleTest dei repository è troppo vecchia, CMake scarica da sola la
versione giusta alla prima configurazione (serve la rete).

## 2. clang-format e clang-tidy versione 18

Il progetto fissa la versione 18 ([ADR 0003](adr/0003-convenzioni.md)), che i
repository Debian non hanno. Si installa dai pacchetti ufficiali pubblicati su
PyPI:

```sh
sudo apt install pipx
pipx install clang-format==18.1.8
pipx install clang-tidy==18.1.8
pipx ensurepath
```

Chiudi la sessione ed entra di nuovo, poi verifica:

```sh
clang-format --version   # deve dire 18.1.8
clang-tidy --version     # deve dire 18.1.8
```

Non installare il pacchetto Debian `clang-format`: una versione diversa
formatterebbe il codice in modo diverso da quello che la CI accetta.

## 3. Antigravity CLI (Gemini) senza conferme continue

Con le impostazioni predefinite Antigravity CLI (`agy`) chiede conferma prima di
ogni scrittura e di ogni comando, e il responsabile deve restare davanti allo
schermo. Si configura una volta, in Debian (anche in WSL), con due accorgimenti.
L'installazione è in [GUIDA-RESPONSABILE.md](GUIDA-RESPONSABILE.md), A4.

**1. Regole dei permessi.** In `~/.gemini/antigravity-cli/settings.json` si
autorizzano i comandi che servono al lavoro e si vietano quelli che spettano solo
al responsabile. Il divieto vince sempre sull'autorizzazione:

```json
{
  "permissions": {
    "allow": [
      "command(git)",
      "command(gh)",
      "command(cmake)",
      "command(ctest)",
      "command(python3)"
    ],
    "deny": [
      "command(regex:^gh pr merge)",
      "command(regex:^gh repo delete)",
      "command(regex:^git push.*(-f|--force))",
      "command(regex:^git push.* main$)"
    ]
  }
}
```

Quando il pacchetto P3 consegna gli script di controllo, si aggiungono qui alla
lista `allow`. Le stesse regole si possono vedere e modificare dentro `agy` con
`/permissions`. La sintassi viene da guide di terze parti e può cambiare tra le
versioni: dopo averle scritte, controlla con `/permissions` che `agy` le legga.

Il file è unico per tutta la macchina, non per progetto: le regole valgono anche
quando `agy` lavora in un'altra cartella. Il confine con Tecnigrafo non si
ottiene con queste regole ma con quelle di
[AGENTS.md](../AGENTS.md) ("Dove puoi scrivere") e con l'avvio dalla cartella
giusta ([GUIDA-RESPONSABILE.md](GUIDA-RESPONSABILE.md), A6).

**2. Modalità automatica e artifact.** Si avvia con:

```sh
agy --mode auto
```

In questa modalità (da `agy` 1.3.5, su Linux e macOS) le modifiche ai file e i
comandi sicuri si approvano da soli, mentre le azioni irreversibili chiedono
ancora conferma. In `/config` si imposta anche la revisione degli artifact
(piani e documenti) su *agent-decides*: altrimenti l'agente si ferma ad
aspettare che il responsabile approvi il piano.

Non usare `--dangerously-skip-permissions` né `toolPermission` su
`always-proceed`: approvano tutto senza chiedere, comprese le azioni
irreversibili, e un testo malevolo letto in una issue o in una pagina web
potrebbe far eseguire comandi qualsiasi.

Nell'IDE Antigravity la lista dei comandi autorizzati è segnalata come
inaffidabile (comandi richiesti anche se autorizzati, voci spostate dopo il
riavvio): per lavorare a lotti si usa la CLI.
