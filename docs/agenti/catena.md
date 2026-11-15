# Catena degli agenti

Si legge insieme al nucleo, [AGENTS.md](../../AGENTS.md), quando sei stato
avviato da un commento su GitHub, o quando il messaggio cita questo file, un
giro di revisione o una «Decisione del responsabile». Vale insieme ai file del
lavoro che fai: qui c'è solo **come il lavoro passa da un agente all'altro**
senza il responsabile ([ADR 0018](../adr/0018-catena-degli-agenti.md)).

Il responsabile avvia un lavoro con **un solo commento** nella issue; gli
agenti si passano il lavoro con commenti scritti dagli script. Lui torna solo
quando trova un'etichetta:

| Etichetta | Che cosa vuol dire | Chi la mette |
|---|---|---|
| `pronta` | lettura finale, DCO e merge | l'ultimo agente della catena |
| `decisione` | serve una sua decisione, che scrive **nella issue** | chi si ferma ("Fermarsi per una decisione") |
| `ferma` | la catena si è interrotta senza dirlo | la sentinella ("Sentinella") |

Non togliere mai `ferma` né `pronta`: le toglie il responsabile.

## Ruoli

Su ogni issue lavorano due agenti:

- l'**esecutore** è quello dell'etichetta `agente:…` della issue
  (`agente:claude-2` è Claude): realizza il lavoro e poi corregge;
- il **revisore** è **l'altro** (Claude se l'esecutore è Codex, e viceversa):
  rivede e non fa mai commit sulla pull request. **Non aspetta la CI** (niente
  `scripts/ci-attendi.sh`): la catena gli passa la pull request solo quando la
  CI è finita, verde per una revisione, rossa dopo tre tentativi per
  un'analisi ("Fine del lavoro").

È il giro a tre di [LAVORO-IN-PARALLELO.md](../LAVORO-IN-PARALLELO.md), "Chi fa
cosa": chi ha scritto i riferimenti di un pacchetto è l'altro agente, e ne
rivede la pull request. Il revisore lavora sulla pull request indicata anche se
la richiesta arriva nella issue che la pull request chiude, e lì prende in
carico, commenta e mette le etichette: per lui non vale il punto 2 del nucleo
("Per prima cosa: prendi in carico"), che riguarda le issue.

## Passaggi: solo con gli script

**Ogni chiocciola davanti al nome di un agente avvia una sessione**, anche in
un esempio o fra apici, perché tutti scrivono con l'account del responsabile.
Quindi non scriverla mai: i passaggi si fanno solo con lo script, che scrive il
testo giusto a nome del responsabile.

```sh
scripts/passa.sh <claude|codex> <numero della pull request> revisione <N>
scripts/passa.sh <claude|codex> <numero della pull request> correzioni <N>
scripts/passa.sh <claude|codex> <numero della pull request> analisi
```

Le richieste di revisione e di analisi per Codex lo script le scrive nella
issue collegata (quella di `Closes #N` o, se manca, di `Avvia dopo il merge:
#N`): su una pull request attiverebbero la sua revisione automatica.

Nella pull request e nei verbali scrivi modello e impegno con cui sei stato
avviato; se non li sai, «modello e impegno non noti».

## Fine del lavoro (esecutore)

Il lavoro **esiste solo su GitHub**: un commit rimasto nel tuo ambiente non lo
vede nessuno, e la catena si ferma senza dirlo. Pubblica sempre il ramo con
`git push` e apri tu la pull request con `gh pr create`, anche se il tuo
strumento propone di «preparare» la pull request in un altro modo. Se `git
push` o `gh pr create` falliscono, scrivi l'errore esatto in un commento nella
issue e fermati ("Fermarsi per una decisione").

Prima di terminare, come **ultima cosa**, dopo i passi del nucleo ("Quando
fermarsi"):

| Lavoro | Con la CI verde | Con la CI rossa dopo tre tentativi |
|---|---|---|
| pacchetto di lavoro (`[X.Y-PN]`) | `scripts/passa.sh <revisore> <numero> revisione 1` | `scripts/passa.sh <revisore> <numero> analisi` |
| correzione del contratto ("Buco nel contratto") | `scripts/passa.sh <revisore> <numero> revisione 1` | come sopra |
| riferimenti (`[X.Y-RN]`) | etichetta `pronta` sulla pull request: li rivede il responsabile ([LAVORO-IN-PARALLELO.md](../LAVORO-IN-PARALLELO.md), "3. Riferimenti") | come sopra |
| chiusura della tappa | `pronta` sulla pull request | come sopra |

Una pull request di **riferimenti** porta nella descrizione la riga
`Avvia dopo il merge: #<issue del pacchetto>`, una per ogni pacchetto di lavoro
che i suoi casi sbloccano: al merge il pacchetto parte da solo, se non aspetta
altro ("Dopo il merge").

## Fermarsi per una decisione

Chiunque tu sia, ti fermi per una decisione in tutti i casi in cui i file del
tuo lavoro dicono di fermarsi e scrivere nella issue o nella pull request: un
caso di riferimento che sembra sbagliato ([pacchetto.md](pacchetto.md), "In un
pacchetto di lavoro"), una pull request da rivedere scritta con il tuo stesso
modello ([revisione.md](revisione.md)), un file che il pacchetto non prevede, un
cambiamento che servirebbe in Tecnigrafo ([altro-repository.md](altro-repository.md)).
In revisione ti fermi anche per una segnalazione **[decisione]** o al giro 3 con
delle [bloccante] ancora aperte. Un buco nel contratto ha la sua strada ("Buco
nel contratto"). Allora, oltre a ciò che chiede il file del tuo lavoro:

1. aggiungi l'etichetta `decisione` alla issue e, se c'è, alla pull request;
2. scrivi **un solo** commento (nella pull request se c'è, altrimenti nella
   issue) che comincia con «Serve una decisione del responsabile», con il
   problema, le scelte possibili e, come ultima riga, quella da copiare,
   **senza la chiocciola**, e **dove** scriverla. Se proponi una correzione
   concreta (un dato, un valore, un caso riscritto), la riga la contiene già
   scritta per intero, così chi corregge applica il dato esatto invece di
   interpretarlo:
   «Per riprendere scrivi **nella issue #<numero>**, con @ davanti:
   `<nome> Decisione del responsabile: …`». Il posto è sempre una **issue**:
   su una pull request la menzione di Codex avvia la sua revisione
   automatica. Nome e issue sono di norma i tuoi; per un caso di riferimento
   sbagliato sono quelli di chi ha scritto i riferimenti e della sua issue
   `[X.Y-RN]`, perché chi realizza un pacchetto non ne tocca i casi. Quella
   issue è già chiusa, perché i riferimenti sono uniti: la riga comincia con
   «Riapri la issue #<numero> e scrivi lì, con @ davanti:» e chiede una pull
   request di soli riferimenti con `Closes #<numero>` e
   `Avvia dopo il merge: #<issue del pacchetto>`. Senza `Closes` la issue dei
   riferimenti resta aperta e il pacchetto, che la aspetta nella sua riga
   «Riferimenti», non riparte ("Dopo il merge");
3. se il commento è nella pull request, scrivi nella issue «Serve una
   decisione del responsabile: vedi la pull request #<numero>»;
4. togli `AA-Presa in carico` e termina, senza aspettare la CI.

## Buco nel contratto

Un buco **visibile** nel contratto (nucleo, "Quando fermarsi") ti ferma come
dice [contratto.md](contratto.md), con la issue `contratto` che apri tu. Nella
catena quella issue fa anche da posto della decisione, e il pacchetto riparte
da solo:

1. la issue `contratto` porta, oltre a `contratto` e `tappa-X.Y`, le etichette
   `decisione` e `agente:` **dell'altro agente**, che scriverà la correzione.
   Il suo testo finisce con la riga da copiare per lui, senza chiocciola:
   «Per riprendere scrivi **in questa issue**, con @ davanti:
   `<altro agente> Decisione del responsabile: …`»;
2. sulla issue del tuo pacchetto metti `decisione` e scrivi un commento
   «Serve una decisione del responsabile: vedi la issue #<contratto>», senza
   riga da copiare: il pacchetto riparte da solo al merge della correzione.
   Se hai lavoro non pubblicato, pubblica il ramo **senza** pull request e
   scrivi nel commento il nome del ramo; togli `AA-Presa in carico` e
   termina.

Alla ripresa l'altro agente è l'esecutore della issue `contratto`: applica la
decisione in **una** pull request come dice [docs/CONTRATTI.md](../CONTRATTI.md),
«Le correzioni del contratto» (intestazione, ADR, `docs/DECISIONI.md`), con in
testa alla descrizione **Revisione del contratto: da fare**, `Closes #<contratto>`
e `Avvia dopo il merge: #<issue del pacchetto>`, e la passa in revisione
("Fine del lavoro"). Il revisore è di nuovo l'altro: chi aveva il pacchetto.
Al merge la issue `contratto` si chiude e il pacchetto riparte.

## Ripresa dopo una decisione

Il responsabile riprende con un commento nella issue che comincia con la
menzione del tuo nome e `Decisione del responsabile:`. Se ti eri fermato in
una pull request lavori lì: la issue è solo il posto da cui riparti. Prendi in
carico, togli `decisione` dalla issue e dalla pull request, e poi:

- **se eri l'esecutore**: continua applicando la decisione, fino alla fine del
  lavoro (o a una nuova decisione). Lo stesso se riparti dopo il merge di una
  correzione del contratto: prima rileggi le intestazioni cambiate
  ([contratto.md](contratto.md), "Prima della pull request");
- **se hai scritto i riferimenti** e la decisione dice che un tuo caso è
  sbagliato: correggilo in una pull request di soli riferimenti con
  `Closes #<issue dei riferimenti>` (riaperta dal responsabile) e
  `Avvia dopo il merge: #<issue del pacchetto>`, come in "Fine del lavoro".
  Prima di aprirla ricontrolla il caso corretto con la scheda e il contratto,
  compreso ogni passo che segue dai dati nuovi: la correzione deve togliere il
  motivo indicato da chi si è fermato, non solo cambiare i dati;
- **se eri il revisore**: scrivi **un solo** commento che comincia con
  «Ripresa dopo la decisione (giro N)», dove N è il giro che si era fermato
  (senza scrivere «Giro di revisione:»: non è un giro nuovo), con ogni modifica
  che la decisione chiede come **[bloccante]** e le [bloccante] di quel giro
  ancora aperte. Se ce n'è almeno una:
  `scripts/passa.sh <esecutore> <numero> correzioni <N>`; altrimenti
  l'etichetta `pronta`. Togli `AA-Presa in carico` e fermati.

## Revisione (revisore)

Prendi in carico la pull request e lascia **un solo commento** che comincia
con «Revisione di <nome dell'agente>», con la riga «Giro di revisione: N». N è
il numero dei commenti della pull request che contengono «Giro di revisione:»,
più uno. Dal giro 2 rivedi **solo i commit di correzione** e le segnalazioni
del giro prima. Che cosa blocca e che cosa no lo dice
[revisione.md](revisione.md); ogni segnalazione comincia con la sua classe:

- **[bloccante]**: un errore vero o una regola persa: la issue o la scheda non
  è soddisfatta, un test manca o è sbagliato, una regola dei documenti degli
  agenti è violata. La risposta è già nella scheda, nel contratto o nelle
  regole;
- **[decisione]**: un buco visibile che nessuna regola esistente decide
  (nucleo, "Buchi visibili e no"; [revisione.md](revisione.md), "Prima di
  dichiarare un buco");
- **[fascia]**: un caso che sta entro un errore dichiarato, da scrivere nel
  commento dell'intestazione ([revisione.md](revisione.md), «La fascia»);
- **[facoltativa]**: tutto il resto, compresi i dubbi di forma, che
  revisione.md chiama «non bloccanti».

Alla fine, in quest'ordine:

- almeno una [decisione], o [bloccante] al giro 3 (scrivi «Fermato dopo tre
  giri»): "Fermarsi per una decisione";
- [bloccante] o [fascia] ai giri 1 o 2, o [fascia] al giro 3:
  `scripts/passa.sh <esecutore> <numero> correzioni <N>`;
- altrimenti (solo [facoltativa] o nessuna segnalazione): etichetta `pronta`.

Poi togli `AA-Presa in carico` e fermati. Non fare commit.

Per una pull request con **Revisione del contratto: da fare** il verbale è
quello di [docs/CONTRATTI.md](../CONTRATTI.md), «Le correzioni del contratto»:
comincia con **Verbale della revisione del contratto** e ha la lista spuntata
per ogni funzione, più la riga «Giro di revisione: N» e le stesse classi. Ogni
buco nuovo visibile è una [decisione], con l'esempio e le scelte possibili,
invece di una issue `contratto` a parte: la decisione entra nella stessa pull
request (lo stesso file, «Il merge»), e non la scegli tu.

## Analisi di una CI rossa (revisore)

Prendi in carico, leggi gli errori con `scripts/ci-errori.sh <numero>` e il
codice, lascia **un solo commento** che comincia con «Analisi di <nome
dell'agente>» con la causa probabile e la correzione che suggerisci. Poi
fermati come in "Fermarsi per una decisione", punti 1, 3 e 4: l'ultima riga del
commento è quella da copiare per l'**esecutore**, nella issue collegata alla
pull request (`Closes #N` o `Avvia dopo il merge: #N`) («`<esecutore>
Decisione del responsabile: applica l'analisi della pull request #<numero>`»),
perché decidere se seguirla spetta al responsabile. Non fare commit.

## Correzioni (esecutore)

Prendi in carico, lavora sul ramo della pull request
(`gh pr checkout <numero>`), correggi solo le **[bloccante]** e le **[fascia]**
del giro indicato e, se c'è, del commento «Ripresa dopo la decisione» di quel
giro (le [facoltativa] e le [decisione] non si toccano). Pubblica i commit con
`git push` sullo stesso ramo ("Fine del lavoro"), aspetta la CI come dice il
nucleo, rispondi con un commento che dice che cosa hai corretto, togli
l'etichetta e, come ultima cosa,
`scripts/passa.sh <revisore> <numero> revisione <N+1>`. Se il giro aveva solo
[fascia], la fascia non chiede un altro giro ([revisione.md](revisione.md),
«La fascia»): con la CI verde metti invece l'etichetta `pronta`.

## Dopo il merge

Al merge di una pull request, il workflow *Dopo il merge*
(`.github/workflows/dopo-il-merge.yml`) legge le righe
`Avvia dopo il merge: #N` della descrizione e avvia la issue N con il suo
agente (`scripts/avvia.sh`), se non aspetta ancora un'altra issue delle sue
righe «Dipende da» e «Riferimenti». Per Claude modello e impegno vengono dalla
riga «Modello e impegno» della issue. Conta solo una **riga a sé**, che
comincia con quelle parole (al più dopo un trattino di elenco) e porta una sola
issue: le stesse parole dentro una frase sono una citazione e non avviano
niente, e le righe dentro un blocco di codice (fra tre backtick o tre tilde)
sono esempi e non contano, nemmeno per `Closes #N` (`scripts/catena.py`).
L'avvio non toglie `decisione`: la toglie l'agente quando prende in carico.
Ogni agente legge l'AGENTS.md del suo ramo: una pull request nata prima di un
cambio di queste regole va aggiornata su `main` prima di entrare nella catena.

## Sentinella

Ogni 30 minuti il workflow *Sentinella* (`.github/workflows/sentinella.yml`)
mette `ferma`, con un commento, sulle issue e pull request della catena
senza movimento da almeno 60 minuti che non portano `pronta` né `decisione`, e
sulle decisioni scritte dal responsabile e mai riprese (per esempio per la
quota di Codex). Una pull request e le issue collegate (`Closes #N`, la riga
`Avvia dopo il merge: #N`) sono una catena sola: i passaggi a Codex scritti
nella issue contano per la pull request. Lascia stare le pull request con la
CI ancora in corso e segnala `AA-Presa in carico` rimasta. Come avvio o
decisione conta solo un commento dell'account del responsabile, che è anche
quello degli agenti; i commenti di altri account non contano.
