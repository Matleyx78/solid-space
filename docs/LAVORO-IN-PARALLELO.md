# Lavoro in parallelo

Come far lavorare più persone o più agenti IA sulla stessa tappa senza che si
intralcino. Il principio è uno: **GitHub è l'unico registro di chi fa cosa**, e
solo il responsabile assegna i pacchetti e unisce le pull request.

La modalità di controllo vale fin dalla tappa 0.1
([ADR 0004](adr/0004-robustezza-e-verifica.md)): chi scrive i riferimenti non è
chi scrive il codice, e un pacchetto che cambia il comportamento è finito quando i
test confermano i casi di verifica scritti prima. La guida pratica, passo per
passo, per il responsabile è in [GUIDA-RESPONSABILE.md](GUIDA-RESPONSABILE.md).

solid-space è un progetto autonomo: tutto ciò che serve per lavorarci sta in
questo repository. Con Tecnigrafo condivide il metodo, non i file (vedi
[GOVERNANCE.md](../GOVERNANCE.md), "Rapporto con Tecnigrafo").

## Come leggere i riferimenti

GitHub usa **una sola numerazione** per issue e pull request: ogni elemento nuovo,
qualunque sia il tipo, prende il numero successivo. Per questo un numero può
indicare una issue o una pull request.

| Scrittura | Significato | Dove si trova |
|---|---|---|
| `[0.2-P3]` | tappa 0.2, pacchetto P3 | titolo della issue del pacchetto |
| `[0.2-R3]` | tappa 0.2, riferimenti del pacchetto P3 | titolo della issue dei riferimenti |
| `[0.3-C]` | tappa 0.3, revisione del contratto ([CONTRATTI.md](CONTRATTI.md)) | titolo della issue della revisione |
| `#5` | issue o pull request numero 5 | scheda **Issues** o **Pull requests** |
| `Closes #5` | la pull request, quando viene unita, chiude la issue 5 | descrizione della pull request |
| `p0.2-03-…` | ramo su cui si lavora al pacchetto P3 della tappa 0.2 | scheda **Branches** |
| `r0.2-03-…` | ramo dei riferimenti del pacchetto P3 della tappa 0.2 | scheda **Branches** |
| `c0.3-…` | ramo della revisione del contratto della tappa 0.3 | scheda **Branches** |

Il documento della tappa (`docs/tappe/…`) riporta per ogni pacchetto il numero
della sua issue; ogni pull request riporta in alto la issue che chiude.

## Chi fa cosa

| Chi | Etichetta | Lavoro |
|---|---|---|
| Responsabile | — | piano della tappa, ADR, revisione dei riferimenti, merge, prova manuale, rilascio |
| Claude | `agente:claude` | pacchetti impegnativi e la maggior parte dei medi, anche in più sessioni nello stesso momento; riferimenti dei pacchetti di lavoro di Codex; piano della tappa insieme al responsabile |
| Codex | `agente:codex` | revisione del contratto di ogni tappa ([CONTRATTI.md](CONTRATTI.md)); riferimenti con calcoli delicati dei pacchetti di lavoro di Claude, in fascia forte; pacchetti facili e medi che il piano non dà a Claude |
| Gemini (con Antigravity CLI, `agy`) | `agente:gemini` | pacchetti di riferimenti ordinari, revisione delle pull request di lavoro, pull request di chiusura della tappa, pacchetti facili; lavora a lotti (sezioni 3 e 4) |
| Gemini 4 Argon (con Antigravity CLI, `agy`) | `agente:gemini-argon` | nessun pacchetto nel piano: la sua quota settimanale basta a uno o due pacchetti di riferimenti. Se è disponibile e serve, il responsabile gli passa un pacchetto che il piano dava a Codex, cambiandone l'etichetta; lavora a lotti, in una sessione sua, sullo stesso PC di Gemini |
| Copilot (GitHub Copilot Pro) | — | solo revisione mirata delle pull request impegnative, chiesta dal responsabile (sezione 6); non prende pacchetti e non apre pull request |

Questi ruoli valgono dalla tappa che corrisponde alla 0.8 di Tecnigrafo; il
piano della tappa 0.2 resta com'è. Più sessioni di Claude nello stesso momento
usano la stessa etichetta: `AA-Presa in carico` basta perché due sessioni non
prendano la stessa issue. Le issue aperte prima con `agente:claude-2` le prende
qualunque sessione di Claude; per le nuove l'etichetta non si usa più.

Chi lavora a un pacchetto non scrive i suoi riferimenti, e viceversa. Le
etichette servono perché gli agenti lavorano tutti attraverso l'account GitHub
del responsabile, e non si possono distinguere in altro modo. La lista delle
issue filtrata per etichetta mostra in ogni momento chi sta facendo cosa.

### Dove può scrivere un agente

Un agente che lavora a solid-space scrive codice e documenti **solo in
solid-space**. In Tecnigrafo e negli altri repository può leggere e, se il
responsabile glielo chiede indicando la issue o la pull request, scrivere un
commento: nient'altro. La regola completa è in [AGENTS.md](../AGENTS.md), "Dove
puoi scrivere"; come farla rispettare, nella guida del responsabile, in
Tecnigrafo:
[GUIDA-RESPONSABILE.md](https://github.com/Matleyx78/tecnigrafo/blob/main/docs/GUIDA-RESPONSABILE.md), sezione A1.

## 1. Preparare il piano

Il piano dettagliato della tappa (modello in [tappe/0000-modello.md](tappe/0000-modello.md))
dice, per ogni pacchetto:

- **Difficoltà**, **agente**, **modello** e **impegno** del lavoro (vedi sotto);
- **Riferimenti**: l'agente che li scrive, oppure "non servono" con il motivo
  (documentazione, note di rilascio, infrastruttura che non cambia il
  comportamento);
- **Contratto**: le intestazioni pubbliche del contratto che il pacchetto
  realizza o usa. Servono agli agenti per riconoscere le issue `contratto` che
  lo bloccano ([AGENTS.md](../AGENTS.md), condizione 5);
- **Dipende da**: le issue che devono essere chiuse prima. Due pacchetti che
  toccano gli stessi file sono **sempre** legati da "Dipende da", così non
  possono partire insieme. Nessuna eccezione: nemmeno l'elenco dei casi in
  attesa è un file condiviso, perché è la cartella `tests/corpus/in-attesa/` con
  un file per blocco di riferimenti, che il pacchetto di riferimenti aggiunge e
  quello di lavoro cancella (ADR 0005, punto 4).

### La revisione del contratto

Una tappa che realizza intestazioni del contratto comincia con il pacchetto
`[X.Y-C]`, **revisione del contratto**: un agente di fascia forte, diverso da chi
ha scritto gli ADR (di norma Codex), controlla ogni funzione con la lista di
[CONTRATTI.md](CONTRATTI.md) e apre una issue `contratto` per ogni buco; il
responsabile decide, e una sola pull request applica le decisioni. Tutte le
issue di riferimenti e di lavoro che realizzano o usano quelle intestazioni lo
citano in «Dipende da». Costa una sessione forte all'inizio della tappa, contro
i pacchetti fermati a metà e le decisioni rincorse tra le chat della tappa 0.2.

### Pacchetti leggeri

Ogni sessione di un agente paga un costo fisso in token prima di scrivere una
riga: legge le regole, il pacchetto, gli ADR e il codice che tocca. Poi ogni
pacchetto costa una CI e un merge. Il piano tiene basso il primo costo e non
moltiplica il secondo:

- **Una scheda per pacchetto.** Ogni pacchetto ha la sua scheda,
  `docs/tappe/X.Y/PN-<nome>.md` (modello in
  [tappe/0000-pacchetto.md](tappe/0000-pacchetto.md)), autosufficiente:
  l'agente legge quella, non il piano intero. Le regole della tappa che lo
  riguardano si ripetono nella scheda, in breve.
- **Un componente per pacchetto**, più i suoi test e le righe dei file comuni.
- **Misura.** Al massimo una quindicina di file e qualche centinaio di righe
  cambiate (indicativamente fino a 600). Un pacchetto più grande si divide, di
  solito lungo i confini naturali (un predicato, un'operazione, una famiglia di
  casi).
- **Non troppo piccoli.** Un pacchetto che tocca una sola funzione o un solo
  file si unisce a un vicino dello stesso componente: ogni pacchetto costa
  comunque una sessione, una CI, una revisione e un merge, e spesso una pull
  request di riferimenti.
- **Riferimenti della stessa misura.** Un pacchetto di riferimenti copre un
  solo pacchetto di lavoro; oltre una ventina di casi si divide per area.

### Modello e impegno

Il piano suggerisce, per ogni pacchetto, la **fascia di modello** e
l'**impegno** (quanto ragionamento chiedere, dove lo strumento lo permette).
Sono indicazioni: il responsabile sceglie il modello quando avvia l'agente. I
nomi dei modelli cambiano, la fascia resta.

La tabella elenca i modelli disponibili al responsabile a ottobre 2026, con il
livello di ragionamento da scegliere. I modelli della stessa riga hanno capacità
simile secondo una misura indipendente dai produttori, l'Intelligence Index di
[Artificial Analysis](https://artificialanalysis.ai/models) (tra parentesi;
più alto è meglio). Quando cambiano i modelli si aggiorna la tabella con la
stessa fonte. La stessa tabella è in Tecnigrafo: chi la aggiorna in un progetto
la allinea anche nell'altro.

| Fascia | Per | Claude | Codex | Gemini |
|---|---|---|---|---|
| **forte** | pacchetti impegnativi: geometria, nuovi componenti, interfacce pubbliche, riferimenti con calcoli delicati | Opus 5.5 medio (51) o alto (54) | astra medium (50) | 4 Argon alto (53), se disponibile |
| **medio** | pacchetti medi: uno schema già presente nel codice, infrastruttura di build e CI | Opus 5.5 medio (51) | sol medium (48) o astra light (46) | — |
| **leggero** | pacchetti facili: documentazione, note di rilascio, piccoli ritocchi; riferimenti ordinari e revisioni | Haiku 5.5 alto (38) o Sonnet 5.5 medio (41) | luna alto (32) o sol light (42) | 3.8 Flash medio (40) o alto (41) |

Gemini 3.8 Flash sta nella fascia leggera: i riferimenti con calcoli delicati
(predicati, tolleranze, casi limite geometrici) li scrive un agente di fascia
forte, diverso da chi farà il pacchetto di lavoro: di norma Codex per i
pacchetti di Claude, e Claude per quelli di Codex. Gemini 4 Argon (53) ha una
capacità da fascia forte secondo la stessa misura, ma una quota settimanale che
basta a uno o due pacchetti di riferimenti: il piano non gli assegna niente, e
il responsabile gli passa un pacchetto quando è disponibile.

Per i pacchetti medi Claude lavora con Opus medio, non con Sonnet: la quota del
piano Max lo permette, e un modello più forte fa meno giri di correzione.

| Impegno | Quando |
|---|---|
| **alto** | il pacchetto richiede di ragionare su casi limite (tolleranze, punti quasi allineati, facce tangenti) o di progettare interfacce |
| **medio** | il lavoro segue uno schema già presente nel codice |
| **basso** | il lavoro è meccanico: testi, elenchi, documenti |

Di norma: facile → leggero e basso; media → medio e medio; impegnativa →
forte e alto. Un pacchetto impegnativo che non si riesce a dividere resta
impegnativo; uno che si divide dà spesso due pacchetti medi.

## 2. Aprire la tappa: issue ed etichette

All'apertura si creano, nel milestone `vX.Y`:

- una issue `[X.Y-PN]` per ogni pacchetto, con le etichette `tappa-X.Y`,
  `pacchetto`, la difficoltà e `agente:…` secondo il piano;
- una issue `[X.Y-RN]` per ogni pacchetto che ha riferimenti, con le etichette
  `tappa-X.Y`, `riferimenti`, la difficoltà e `agente:…` di chi li scrive. La
  issue del lavoro la cita nella riga "Dipende da";
- se la tappa realizza intestazioni del contratto, la issue `[X.Y-C]` della
  revisione, con le etichette `tappa-X.Y`, `pacchetto`, `contratto`,
  `difficoltà:impegnativa` e `agente:…` di chi la fa, se non esiste già (di
  norma si crea prima, appena gli ADR sono accettati: [CONTRATTI.md](CONTRATTI.md),
  «Quando»). Le issue di riferimenti e di lavoro che realizzano o usano quelle
  intestazioni la citano in «Dipende da».

La difficoltà (`facile`, `media` o `impegnativa`) è quella della colonna
*Difficoltà* del piano: chi apre la tappa la copia, non la decide. Serve a
scegliere l'agente quando ce n'è più d'uno disponibile. Tecnigrafo usa le
stesse etichette.

Il testo di ogni issue cita la scheda del pacchetto (`docs/tappe/X.Y/PN-<nome>.md`)
e ne riporta modello e impegno, così l'agente parte da lì.

Le etichette del progetto sono `idea`, `pacchetto`, `riferimenti`, `tappa-X.Y`,
`facile`, `media`, `impegnativa`, `agente:claude`, `agente:codex`,
`agente:gemini`, `agente:gemini-argon` e `AA-Presa in carico` (sezione 5).
`agente:claude-2` resta solo sulle issue aperte prima della tappa che
corrisponde alla 0.8 di Tecnigrafo: le prende qualunque sessione di Claude, e
l'etichetta si elimina quando non ne restano. `agente:gemini-argon` lo mette il
responsabile sulle issue che passa a Gemini 4 Argon, quando ha quota.

Le issue le può creare Claude, partendo dal piano, quando il responsabile glielo
chiede.

### Le dipendenze

Le dipendenze di una issue si segnano in due modi, che dicono la stessa cosa:

- la riga «Dipende da» del testo, che fa fede per gli agenti
  ([AGENTS.md](../AGENTS.md), "Se ti chiedono di prendere il prossimo
  pacchetto");
- la relazione nativa «bloccata da» di GitHub, che le mostra nelle liste e nel
  Project.

Servono i numeri delle issue, quindi chi apre la tappa procede in quest'ordine:

1. crea tutte le issue, nell'ordine del piano (in cui ogni dipendenza viene
   prima di chi la richiede);
2. riscrive il testo di ognuna con i numeri nella riga «Dipende da»;
3. per ogni dipendenza segna «bloccata da», **solo tra issue di solid-space**:

   ```sh
   gh api repos/Matleyx78/solid-space/issues/<m>/dependencies/blocked_by \
     -F issue_id=<id>
   gh api repos/Matleyx78/solid-space/issues/<n> --jq .id   # l'<id> di una issue
   ```

   `<m>` è la issue che aspetta e `<id>` l'id numerico della issue che la
   blocca, che non coincide con il suo numero `<n>`.

Se GitHub rifiuta una dipendenza non si ferma nulla: resta la riga «Dipende
da» e chi apre la tappa lo scrive al responsabile. Quando si aggiungono
pacchetti a tappa aperta, le dipendenze si segnano solo per le issue nuove.

## 3. Riferimenti

1. L'agente dei riferimenti prende una issue `[X.Y-RN]` alla volta, scrive i casi
   di verifica e apre una pull request (regole in [AGENTS.md](../AGENTS.md),
   sezione "Robustezza e verifica").
2. Il responsabile la rivede: è il punto in cui la sua attenzione rende di più.
   Controlla che ogni condizione "Fatto quando" del pacchetto abbia un caso, che
   i valori attesi siano giusti, e che i file presi dal web abbiano una licenza
   adatta (la CI controlla l'hash, non la licenza).
3. Unita la pull request, il pacchetto di lavoro collegato può partire.

I pacchetti di riferimenti non dipendono l'uno dal merge dell'altro, quindi
l'agente li fa **a lotti**: in una sola sessione prende di fila tutti quelli
pronti, con un ramo e una pull request per ciascuno (regola in
[AGENTS.md](../AGENTS.md), "Lavoro a lotti"). Il responsabile li rivede poi uno
alla volta, come sopra.

## 4. Avviare l'agente

Il messaggio normale è sempre lo stesso: l'agente sceglie da solo il pacchetto
giusto con le regole di [AGENTS.md](../AGENTS.md) ("Se ti chiedono di prendere il
prossimo pacchetto").

```text
Lavora sul repository Matleyx78/solid-space. Leggi AGENTS.md e seguilo alla
lettera. Prendi il prossimo pacchetto della tappa 0.2 assegnato a te, secondo le
regole di AGENTS.md. Realizzalo, apri la pull request, porta la CI al
verde e fermati, come dice AGENTS.md.
```

Per Gemini i messaggi sono a lotti: una sessione fa tutto il lavoro pronto, e
il responsabile non deve riavviarla per ogni pacchetto.

```text
Lavora sul repository Matleyx78/solid-space. Leggi AGENTS.md e seguilo alla
lettera. Prendi i pacchetti di riferimenti pronti della tappa 0.2 assegnati a
te, uno dopo l'altro, come dice AGENTS.md in "Lavoro a lotti". Per ognuno apri
la pull request e porta la CI al verde; alla fine scrivimi un solo riepilogo.
```

```text
Lavora sul repository Matleyx78/solid-space. Leggi AGENTS.md e seguilo alla
lettera. Rivedi le pull request aperte, come dice AGENTS.md in "Lavoro a
lotti", e alla fine scrivimi un solo riepilogo.
```

Perché Gemini lavori senza chiedere conferma a ogni comando, Antigravity CLI va
configurato una volta: istruzioni in
[AMBIENTE-SVILUPPO.md di Tecnigrafo](https://github.com/Matleyx78/tecnigrafo/blob/main/docs/AMBIENTE-SVILUPPO.md), sezione 6.

Se serve far partire un pacchetto preciso, si indicano issue e ramo:

```text
Lavora sul repository Matleyx78/solid-space. Leggi AGENTS.md e seguilo alla
lettera. Realizza solo il pacchetto P3 della tappa 0.2 (issue #12), che è
assegnato a te. Lavora sul ramo p0.2-03-orient2d, partendo da main
aggiornato. Alla fine apri una pull request che chiuda la issue #12, porta la CI
al verde e fermati, come dice AGENTS.md.
```

Mai dare compiti generici ("continua il progetto", "sistema quello che serve"):
un agente senza pacchetto preciso sconfina.

## 5. Mentre lavorano

- Ogni agente lavora **solo sul suo ramo** e apre **una sola pull request**.
- Nessuno scrive su `main` o sui rami degli altri.
- Se un agente scopre che gli serve un file non previsto, si ferma e lo scrive
  nella issue: decide il responsabile.
- Se un agente trova un buco nel contratto, si ferma prima di scrivere codice o
  casi e apre una issue `contratto` ([CONTRATTI.md](CONTRATTI.md), «I buchi
  trovati più tardi»). Finché è aperta, nessun pacchetto che realizza o usa
  quell'intestazione parte; quando la decisione cambia l'intestazione, chi la
  applica avvisa con un commento i pacchetti già in lavorazione su di essa.
- Se un agente ritiene sbagliato un riferimento, si ferma e lo scrive nella
  issue. La correzione è una nuova pull request di riferimenti, mai una modifica
  nella pull request di lavoro.
- Chi prende in carico una issue (per realizzarla) o una pull request già aperta
  (per rivederla, correggerla o risolvere un conflitto) le mette **per prima
  cosa** l'etichetta `AA-Presa in carico` con un commento che dice chi è, e la
  toglie quando ha finito. Una issue o una pull request con quell'etichetta non
  si tocca: ci sta già lavorando qualcun altro. I comandi sono in
  [AGENTS.md](../AGENTS.md).
- Nessun agente scrive in Tecnigrafo, salvo un commento richiesto dal
  responsabile ([AGENTS.md](../AGENTS.md), "Dove puoi scrivere").

## 6. Unire

- Il responsabile unisce **una pull request alla volta**, con *Squash and merge*,
  solo con la CI verde. Prima guarda la scheda **Files changed**: i file devono
  essere quelli del pacchetto, e una pull request di lavoro non deve toccare i
  casi di `tests/corpus/` che non sono suoi (la CI lo controlla, dalla consegna
  del pacchetto P3 della tappa 0.1).
- Dopo ogni merge, le altre pull request aperte mostrano il pulsante **Update
  branch**: si preme, la pull request si allinea a `main` e la CI riparte.
- Se due pacchetti toccano lo stesso file comune (per esempio
  `tests/CMakeLists.txt`), vale una regola fissa: **chi arriva dopo si aggiorna**.
  Si chiede all'agente di quella pull request di risolvere il conflitto.
- Quando possibile, una pull request scritta da un modello viene rivista da un
  modello diverso prima del merge: di norma Gemini rivede le pull request di
  Claude e di Codex. Le pull request impegnative di Claude il responsabile può
  farle rivedere anche a Copilot, che consuma i crediti mensili di Copilot Pro:
  solo dove una seconda opinione forte conta (guida del responsabile, in
  Tecnigrafo:
  [GUIDA-RESPONSABILE.md](https://github.com/Matleyx78/tecnigrafo/blob/main/docs/GUIDA-RESPONSABILE.md),
  B9). La decisione resta del responsabile.

## 7. Chiudere un pacchetto

Con il merge la issue si chiude da sola (`Closes #…`). Il responsabile toglie
l'etichetta `agente:…` solo se il pacchetto viene abbandonato senza pull request:
dopo 14 giorni senza notizie il pacchetto torna libero
([CONTRIBUTING.md](../CONTRIBUTING.md)).

## 8. Chiudere la tappa

Quando tutte le issue del milestone sono chiuse, un agente (di norma Gemini) apre
la **pull request di chiusura**, sul ramo `docs/chiusura-X.Y`, che:

- spunta nel documento della tappa ogni criterio di chiusura, indicando per
  ciascuno i casi di verifica o i test che lo provano, oppure "da provare a mano";
- scrive le note di rilascio `docs/rilasci/X.Y.md` (la cartella nasce con la
  prima chiusura), con la tabella della prova manuale da compilare;
- aggiorna lo stato in [ROADMAP.md](ROADMAP.md) e nel [README](../README.md).

Il responsabile esegue la prova manuale (sezione 7 del documento della tappa),
compila la tabella, unisce la pull request e pubblica la versione (regole in
[ROADMAP.md](ROADMAP.md), "Chiudere una tappa").
