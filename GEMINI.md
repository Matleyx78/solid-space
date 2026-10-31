# solid-space

Le istruzioni per gli assistenti IA sono in [AGENTS.md](AGENTS.md): leggile per
intero prima di qualunque modifica e seguile alla lettera.

Questo file lo leggono Gemini CLI e Antigravity CLI. Di norma Gemini lavora ai
**pacchetti di riferimenti** (`[X.Y-RN]`): le regole sono in AGENTS.md, sezione
"Robustezza e verifica", e in
[docs/adr/0004-robustezza-e-verifica.md](docs/adr/0004-robustezza-e-verifica.md).
Fa anche le revisioni delle pull request, la pull request di chiusura della
tappa e i pacchetti facili, come la documentazione.

Se sei **Gemini 4 Argon**, il responsabile te lo dice nel messaggio: la tua
etichetta è `agente:gemini-argon`, non `agente:gemini`, e la tua cartella di
build è `build-gemini-argon` (`SS_BUILD_SUFFIX=-gemini-argon`). Prendi solo le
issue con la tua etichetta: quelle di `agente:gemini` sono di Gemini 3.8 Flash,
che lavora sullo stesso PC. Il piano non ti assegna niente: l'etichetta la
mette il responsabile sulle issue che ti passa, quando hai quota disponibile.

Riferimenti e revisioni si fanno **a lotti**: una sessione prende di fila tutto
il lavoro pronto, con una pull request per pacchetto (AGENTS.md, "Lavoro a
lotti").

Scrivi codice e documenti **solo in solid-space**. In Tecnigrafo leggi, e scrivi
un commento solo se l'utente te lo chiede indicando la issue o la pull request
(AGENTS.md, "Dove puoi scrivere").
