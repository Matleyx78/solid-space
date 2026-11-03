# solid-space

Le istruzioni per gli assistenti IA sono in [AGENTS.md](AGENTS.md): leggi sempre
il nucleo, cioè AGENTS.md, e i file di [docs/agenti/](docs/agenti/) che la sua
tabella «Se ... leggi anche ...» indica per il tuo lavoro; seguili alla lettera.

Questo file lo leggono Gemini CLI e Antigravity CLI. Di norma Gemini lavora ai
**pacchetti di riferimenti** (`[X.Y-RN]`): le regole sono in
[docs/agenti/riferimenti.md](docs/agenti/riferimenti.md) e in
[docs/adr/0004-robustezza-e-verifica.md](docs/adr/0004-robustezza-e-verifica.md).
Fa anche le revisioni delle pull request
([docs/agenti/revisione.md](docs/agenti/revisione.md)), la pull request di
chiusura della tappa ([docs/agenti/chiusura.md](docs/agenti/chiusura.md)) e i
pacchetti facili, come la documentazione.

Se sei **Gemini 4 Argon**, il responsabile te lo dice nel messaggio: la tua
etichetta è `agente:gemini-argon`, non `agente:gemini`, e la tua cartella di
build è `build-gemini-argon` (`SS_BUILD_SUFFIX=-gemini-argon`). Prendi solo le
issue con la tua etichetta: quelle di `agente:gemini` sono di Gemini 3.8 Flash,
che lavora sullo stesso PC. Il piano non ti assegna niente: l'etichetta la
mette il responsabile sulle issue che ti passa, quando hai quota disponibile.

Riferimenti e revisioni si fanno **a lotti**: una sessione prende di fila tutto
il lavoro pronto, con una pull request per pacchetto
([docs/agenti/lotti.md](docs/agenti/lotti.md)).

Scrivi codice e documenti **solo in solid-space**. In Tecnigrafo leggi, e scrivi
un commento solo se l'utente te lo chiede indicando la issue o la pull request
(AGENTS.md, "Dove puoi scrivere").
