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

## 3. Antigravity CLI (Gemini)

Antigravity si installa e si configura una volta per PC, con una sola lista di
permessi valida per Tecnigrafo e solid-space. Le istruzioni sono in
[AMBIENTE-SVILUPPO.md di Tecnigrafo](https://github.com/Matleyx78/tecnigrafo/blob/main/docs/AMBIENTE-SVILUPPO.md), sezione 6.
Qui restano gli strumenti per la build di solid-space, nelle sezioni 1 e 2.
