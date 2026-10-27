# Guida del responsabile, passo per passo

Questa guida è per il responsabile del progetto. Dice **cosa fare, in che
ordine, e cosa scrivere agli agenti**, senza dover ricordare le regole. Le regole
complete sono in [LAVORO-IN-PARALLELO.md](LAVORO-IN-PARALLELO.md),
[ROADMAP.md](ROADMAP.md) e nell'[ADR 0004](adr/0004-robustezza-e-verifica.md).

Come si usa:

- la **parte A** si fa una volta sola;
- la **parte B** si ripete a ogni tappa, dall'inizio alla fine;
- la **parte C** serve quando qualcosa non va;
- le frasi da copiare sono nei riquadri grigi: si copiano così come sono,
  cambiando solo i numeri tra `< >`.

Cartella di lavoro sul PC: `~/progetti/solid-space`. Ogni agente si avvia **da
quella cartella** e da nessun'altra (vedi A6).

---

## A. Una volta sola

### A1. Unire la pull request di questa guida

1. Apri la pull request su GitHub e leggila.
2. Premi **Squash and merge**.
3. Nel messaggio che compare, controlla che l'ultima riga sia:

   ```text
   Signed-off-by: Matleyx78 <18114626+Matleyx78@users.noreply.github.com>
   ```

   Se manca, aggiungila tu. Vale per **ogni** squash and merge.
4. Premi **Confirm squash and merge**.

### A2. Le etichette

Servono queste etichette (GitHub: **Issues** → **Labels** → **New label**). Sono
già state create tutte, tranne `tappa-X.Y`, che nasce all'apertura di ogni tappa
(B2): apri l'elenco e controlla che ci siano.

| Etichetta | A cosa serve |
|---|---|
| `idea` | proposta per le tappe future: si discute, non si implementa |
| `pacchetto` | issue di un pacchetto di lavoro (`[X.Y-PN]`) |
| `riferimenti` | issue dei casi di verifica di un pacchetto (`[X.Y-RN]`) |
| `tappa-X.Y` | tappa a cui appartiene la issue (una per tappa, creata all'apertura) |
| `agente:claude`, `agente:claude-2`, `agente:codex`, `agente:gemini` | a chi è assegnata la issue |
| `AA-Presa in carico` | qualcuno ci sta lavorando adesso: non toccare |

### A3. Due impostazioni del repository

1. **Settings** → **General** → sezione **Pull Requests**.
2. Spunta **Automatically delete head branches**: i rami si cancellano da soli
   dopo il merge, e la scheda *Branches* resta pulita.
3. Facoltativo ma consigliato, **da fare quando il pacchetto P3 della tappa 0.1
   è unito** (prima non esistono controlli da richiedere): **Settings** →
   **Rules** → **Rulesets** → **New ruleset** → **New branch ruleset**.
   - Nome: `main protetto`. **Enforcement status:** *Active*.
   - **Target branches** → **Add target** → *Include default branch*.
   - Spunta **Require status checks to pass** e aggiungi, uno per uno, i
     controlli che la CI mostra in una pull request. I nomi sono nell'[ADR
     0006](adr/0006-build-ci-e-sottomoduli.md), punto 5: `Formatting (clang-format
     18)`, `Linux (GCC, clang-tidy)`, `Windows (MSVC)`, `macOS (Apple Clang)`,
     `Linux (ASan, UBSan)` e `Reference separation (ADR 0004)`.
   - Nella sezione **Bypass list** aggiungi *Repository admin*, così in caso di
     emergenza puoi ancora unire tu.
   - **Create**.

   Da quel momento GitHub non ti lascia unire una pull request con la CI rossa
   per distrazione.

### A4. Installare Gemini (Antigravity CLI)

Gemini lavora come Claude Code e Codex: un programma nel terminale, dentro la
cartella del progetto, che usa il tuo account GitHub. Il programma si chiama
**Antigravity CLI** e il comando è `agy`. Si installa una volta per PC (Debian
dell'ufficio e Debian in WSL a casa, nello stesso modo): se l'hai già fatto per
un altro progetto, salta ai punti 5–9.

1. Se avevi installato il vecchio Gemini CLI, toglilo, così non si confonde:

   ```sh
   npm uninstall -g @google/gemini-cli
   ```

   Se risponde che non c'era, va bene lo stesso.

2. Installa Antigravity CLI. Non serve `sudo`: il programma va nella tua cartella
   personale (`~/.local/bin/agy`).

   ```sh
   curl -fsSL https://antigravity.google/cli/install.sh | bash
   ```

3. **Chiudi il terminale e aprine uno nuovo**, poi controlla:

   ```sh
   which agy
   ```

   Deve rispondere con un percorso che finisce in `.local/bin/agy`. Se non
   risponde niente:

   ```sh
   echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc && source ~/.bashrc
   ```

   e riprova `which agy`.

4. Controlla che GitHub risponda con il tuo account:

   ```sh
   gh auth status
   ```

   Se dice che non sei collegato: `gh auth login` e segui le domande (GitHub.com,
   HTTPS, accesso con il browser).

5. Entra nella cartella del progetto e avvia Antigravity:

   ```sh
   cd ~/progetti/solid-space
   git checkout main && git pull
   agy
   ```

6. Alla prima apertura ti chiede colori e modo di visualizzazione (va bene quello
   proposto) e se ti fidi della cartella: rispondi **sì**, è la tua.

7. Accesso con il tuo account Google. In WSL il browser potrebbe non aprirsi:
   allora il terminale mostra un indirizzo. Copialo nel browser di Windows,
   accedi, copia il **codice** che ti mostra e incollalo nel terminale. Il piano
   gratuito ha un limite di uso settimanale: lo vedi con `/usage`.

8. **Controlla il modello.** Il piano gratuito offre anche modelli Claude e GPT,
   ma per i riferimenti serve un modello **Gemini**, diverso da quelli che
   scrivono il codice. Il modello in uso è scritto in basso a destra. Per
   cambiarlo scrivi `/model`, scegli **Gemini 3.8 Flash** e con le frecce
   `←` `→` porta *Effort* su **medium**. Poi `Invio`. È il modello della fascia
   leggera nella tabella di
   [LAVORO-IN-PARALLELO.md](LAVORO-IN-PARALLELO.md) ("Modello e impegno"): quando
   cambiano i modelli disponibili, segui quella tabella.

9. Prima prova. Scrivi:

   ```text
   Leggi GEMINI.md e AGENTS.md. Spiegami in cinque righe cosa devi fare e cosa
   non devi mai fare in un pacchetto di riferimenti, e dove puoi scrivere.
   ```

   Se la risposta parla di casi di verifica, di non eseguire solid-space per i
   valori attesi, di licenze e dice che puoi scrivere solo in solid-space, è
   pronto.

### A5. Configurare Antigravity per lavorare da solo

Con le impostazioni predefinite Antigravity chiede il permesso prima di ogni
comando e di ogni modifica, e devi restare davanti allo schermo. Si configura una
volta su ogni PC: regole dei permessi, `agy --mode auto` e revisione degli
artifact su *agent-decides*. Passi e motivi sono in
[AMBIENTE-SVILUPPO.md](AMBIENTE-SVILUPPO.md), sezione 3. Dopo averle scritte,
dentro `agy` scrivi `/permissions` e controlla che le regole compaiano.

Non usare mai `--dangerously-skip-permissions` né *always-proceed*: approvano
tutto senza chiedere, comprese le azioni irreversibili.

### A6. Il confine con Tecnigrafo

solid-space è un progetto autonomo, legato a Tecnigrafo ma con le sue regole. La
regola per gli agenti è una sola ([AGENTS.md](../AGENTS.md), "Dove puoi
scrivere"):

- **in solid-space** scrivono codice e documenti, entro il loro pacchetto;
- **in Tecnigrafo** leggono, e scrivono **un commento** su una issue o pull
  request solo se glielo chiedi tu, indicando quale. Niente file, rami, commit,
  pull request, etichette, chiusure.

La regola scritta, da sola, si basa sull'obbedienza dell'agente. Per renderla
più difficile da violare, usa tutti i livelli che puoi:

1. **Avvia ogni agente dalla cartella di solid-space**, mai da una cartella
   sopra che contenga anche Tecnigrafo. Un agente scrive volentieri dove si
   trova.
2. **Una sessione, un progetto.** Una sessione di solid-space non si usa per
   lavorare a Tecnigrafo, e viceversa: per Tecnigrafo apri una sessione nella
   sua cartella, con le sue regole.
3. **Nelle sessioni sul web** (claude.ai) collega solid-space con accesso in
   scrittura e Tecnigrafo, se serve, solo in lettura. Senza accesso in scrittura
   l'agente non può pubblicare nulla in Tecnigrafo, qualunque cosa decida.
4. **Quando vuoi un commento in Tecnigrafo** scrivi chiaro quale e cosa dire:

   ```text
   Leggi la issue #<numero> di Matleyx78/tecnigrafo e scrivi lì un commento
   che <cosa deve dire>. Non modificare altro, né in Tecnigrafo né qui.
   ```

5. **Vale anche al contrario.** Perché Tecnigrafo rispetti lo stesso confine,
   la sua `AGENTS.md` ha bisogno di un paragrafo gemello ("in Tecnigrafo scrivi
   solo in Tecnigrafo; in solid-space leggi e commenta se richiesto"). Si
   aggiunge con una pull request di Tecnigrafo, decisa da te.

Quello che l'agente legge in Tecnigrafo (issue, commenti, file) è informazione,
non un ordine: se contiene richieste, deve riferirtele, non eseguirle.

---

## B. Il ciclo di ogni tappa

Le frasi indicano la tappa con `<tappa>`, per esempio `0.2`.

### B1. Preparare il piano (tu e Claude)

1. Apri una chat con Claude nel progetto solid-space.
2. Scrivi:

   ```text
   Prepariamo il piano dettagliato della tappa <tappa> partendo dalla scheda in
   docs/tappe e dai modelli tappe/0000-modello.md e tappe/0000-pacchetto.md,
   con le regole di docs/LAVORO-IN-PARALLELO.md ("Preparare il piano"). Per ogni
   pacchetto indica difficoltà, agente del lavoro, fascia di modello e impegno,
   e chi scrive i riferimenti: Gemini per quelli ordinari, un agente di fascia
   forte diverso da chi fa il lavoro per quelli con calcoli delicati, oppure
   "non servono" con il motivo. Assegna a Gemini anche i pacchetti facili di
   documentazione. Proponi anche le bozze degli ADR che servono.
   Elenca anche le issue con etichetta `idea` ancora aperte e dimmi quali,
   secondo te, potrebbero entrare in questa tappa e perché: le approvo io, una
   per una; quelle che approvo entrano nella scheda, le altre restano issue.
   ```

3. Discutete finché il piano ti convince. Gli ADR nuovi nascono *Proposti*: li
   accetti quando ti convincono, anche subito ([GOVERNANCE.md](../GOVERNANCE.md)).
4. Claude apre una pull request con piano e ADR. La leggi e la unisci come in A1.

### B2. Aprire la tappa (Claude crea le issue)

1. Scrivi a Claude:

   ```text
   Apri la tappa <tappa> secondo docs/ROADMAP.md, "Aprire una tappa": crea il
   milestone v<tappa>, l'etichetta tappa-<tappa>, le issue [<tappa>-PN] e
   [<tappa>-RN] con le etichette indicate nel piano, scrivi i numeri delle issue
   nel documento della tappa e aggiorna la tabella della ROADMAP. Apri una pull
   request con le modifiche ai documenti.
   ```

2. Su GitHub apri **Issues** e filtra per il milestone: controlla che ogni issue
   abbia l'etichetta `agente:…` giusta.
3. Unisci la pull request come in A1.

### B3. Riferimenti (Gemini, a lotti)

Gemini scrive i riferimenti ordinari. Quelli con calcoli delicati il piano li
assegna a un agente di fascia forte (Claude o Codex) diverso da chi farà il
lavoro: per quelli si usa la frase di B4, e la revisione al punto 3 è la stessa.

1. Nel terminale, nella cartella del progetto:

   ```sh
   cd ~/progetti/solid-space && git checkout main && git pull && agy --mode auto
   ```

2. Scrivi:

   ```text
   Lavora sul repository Matleyx78/solid-space. Leggi AGENTS.md e seguilo alla
   lettera. Prendi i pacchetti di riferimenti pronti della tappa <tappa>
   assegnati a te, uno dopo l'altro, come dice AGENTS.md in "Lavoro a lotti".
   Per ognuno apri la pull request e porta la CI al verde; alla fine scrivimi
   un solo riepilogo.
   ```

   Gemini lavora da solo finché i pacchetti pronti non finiscono. Puoi
   allontanarti: alla fine trovi il riepilogo con tutte le pull request.

3. Rivedi tu **ogni** pull request del lotto, una alla volta. È il controllo più
   importante di tutta la tappa. Guarda queste cinque cose:
   - [ ] ogni condizione "Fatto quando" del pacchetto ha almeno un caso;
   - [ ] i valori attesi hanno senso (per esempio: un cubo di lato 1 ha volume 1,
         area 6 e 6 facce) e non possono venire da solid-space;
   - [ ] ci sono casi limite (valori nulli, punti coincidenti o quasi allineati,
         facce tangenti, spessori minimi);
   - [ ] per ogni file preso dal web c'è il link alla licenza, e la licenza
         permette di usarlo (MIT, BSD, CC0, CC BY, GPL 3 vanno bene; "solo uso
         personale" o nessuna licenza no);
   - [ ] la pull request tocca solo `tests/corpus/`.
4. Se qualcosa non va, scrivilo come commento nella pull request e di' a Gemini:

   ```text
   Leggi i commenti della pull request #<numero> e correggi.
   ```

5. Se va bene, unisci come in A1.

### B4. Lavoro (Claude e Codex)

Ogni agente in una sessione sua. Possono lavorare nello stesso momento.

1. Avvia l'agente (Claude Code o Codex) nella cartella del progetto, dopo
   `git checkout main && git pull`. Scegli il modello e l'impegno che il piano
   indica per il pacchetto (tabella in
   [LAVORO-IN-PARALLELO.md](LAVORO-IN-PARALLELO.md), "Modello e impegno").
2. Scrivi:

   ```text
   Lavora sul repository Matleyx78/solid-space. Leggi AGENTS.md e seguilo alla
   lettera. Prendi il prossimo pacchetto della tappa <tappa> assegnato a te,
   secondo le regole di AGENTS.md. Realizzalo, apri la pull request, porta la CI
   al verde e fermati, come dice AGENTS.md.
   ```

3. Se l'agente risponde che nessun pacchetto è pronto, è normale: vuol dire che
   aspetta un merge (una dipendenza o i suoi riferimenti). Riprovi dopo.

Per te non cambia niente nei dettagli della CI: conta solo la CI verde della
pull request.

### B5. Seconda opinione (Gemini, a lotti)

Quando ci sono alcune pull request di lavoro aperte, in una sessione di
Antigravity (`agy --mode auto`):

```text
Lavora sul repository Matleyx78/solid-space. Leggi AGENTS.md e seguilo alla
lettera. Rivedi le pull request aperte, come dice AGENTS.md in "Lavoro a
lotti", e alla fine scrivimi un solo riepilogo.
```

Gemini lascia un commento di revisione in ognuna, senza modificare file. Leggi i
commenti prima di unire (B6): se uno chiede correzioni che condividi, passalo
all'agente che ha scritto la pull request.

### B5.1. Segnalazione errori

Quando un agente trova un errore nei riferimenti:

```text
Lavora sul repository Matleyx78/solid-space. Leggi AGENTS.md e seguilo alla
lettera. Controlla la issue #<numero> e leggi i commenti. L'agente che se ne
occupa segnala un errore. Se ha ragione, correggi; altrimenti commenta
spiegando il perché.
```

### B6. Unire

1. Apri la pull request. In basso tutti i controlli devono essere verdi.
2. Scheda **Files changed**: i file devono essere quelli del pacchetto.
3. Squash and merge come in A1, con la riga `Signed-off-by`.
4. Se ci sono altre pull request aperte, su ognuna premi **Update branch**.
5. Riavvia l'agente che ha finito con la stessa frase di B4: prenderà il
   pacchetto successivo. Gemini invece non va riavviato dopo ogni merge: rilancia
   il lotto di B3 quando un merge ha sbloccato nuovi pacchetti di riferimenti
   (la loro riga "Dipende da" è tutta chiusa), e quello di B5 quando si sono
   accumulate nuove pull request da rivedere.

Ripeti B3–B6 finché nel milestone non restano issue aperte.

### B7. Chiudere la tappa

1. A Gemini:

   ```text
   Lavora sul repository Matleyx78/solid-space. Leggi AGENTS.md. Tutte le issue
   del milestone v<tappa> sono chiuse: prepara la pull request di chiusura della
   tappa come descritto in docs/LAVORO-IN-PARALLELO.md, "Chiudere la tappa", sul
   ramo docs/chiusura-<tappa>. Poi fermati.
   ```

2. Leggi la pull request: per ogni criterio di chiusura deve esserci la prova (un
   caso di verifica o un test), oppure "da provare a mano".
3. Fai la **prova manuale** descritta nelle note di rilascio e scrivi l'esito
   nella tabella (puoi chiedere all'agente di compilarla per te, dandogli gli
   esiti).
4. Unisci come in A1.
5. Nel terminale:

   ```sh
   git checkout main && git pull
   git tag -a v<tappa>.0 -m "solid-space <tappa>.0"
   git push origin v<tappa>.0
   ```

6. Su GitHub: **Releases** → **Draft a new release** → scegli il tag
   `v<tappa>.0` → copia nel testo le note di `docs/rilasci/<tappa>.md` →
   **Publish release**.
7. Torna a B1 per la tappa successiva.

---

## C. Se qualcosa va storto

| Cosa succede | Cosa fai |
|---|---|
| La CI è rossa | Non unire. Scrivi all'agente: `La CI della pull request #<numero> è rossa: leggi l'errore e correggi.` |
| Fallisce il controllo di separazione tra riferimenti e lavoro | La pull request mescola riferimenti e codice. Scrivi all'agente: `Dividi la pull request #<numero> in due, come chiede ADR 0004.` |
| Un agente dice che un riferimento è sbagliato | Leggi il suo commento nella issue. Se ha ragione, fai correggere il riferimento a Gemini (B3, indicando la issue); se ha torto, rispondi nella issue e fallo continuare. Per farlo, usa la frase di B5.1. |
| Conflitto con `main` | Scrivi all'agente di quella pull request: `Aggiorna la pull request #<numero> con main e risolvi i conflitti.` |
| Una issue ha `AA-Presa in carico` ma nessuno ci lavora più | Togli l'etichetta a mano e riavvia l'agente. |
| Un agente non risponde da 14 giorni | Il pacchetto torna libero: cambi l'etichetta `agente:…` e lo assegni a un altro. |
| Un agente ha scritto o proposto qualcosa in Tecnigrafo senza che tu l'abbia chiesto | Non unire e non rispondere. Chiudi la pull request o cancella il commento, e di' all'agente: `La regola "Dove puoi scrivere" di AGENTS.md vieta di scrivere in Tecnigrafo: spiega cos'è successo.` |
| Gemini si ferma a chiedere conferma per un comando | Se il comando serve al lavoro ed è sicuro (legge, compila, prova), approvalo e aggiungilo alle regole con `/permissions`, così non lo chiede più. Se cancella, unisce o pubblica su `main`, rifiuta. Regole in A5. |
| Gemini ha finito il limite di uso (`/usage`) | Riprendi quando il limite si azzera, oppure fai scrivere quei riferimenti a un agente che **non** lavora a quel pacchetto. |
| Non sai cosa fare | Apri una chat con Claude nel progetto e incolla questa riga: `Sono al passo <passo> della guida e succede questo: …` |
