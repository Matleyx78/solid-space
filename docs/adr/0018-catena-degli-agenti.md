# ADR 0018: Catena degli agenti avviata con un commento

- **Stato:** Accettato
- **Data:** 2026-11-15
- **Corretto:** 2026-11-28 — punto 4: una [decisione] passa prima dallo
  smistamento dell'esecutore, che risolve con le regole esistenti quelle che
  una regola decide; ferma solo ciò che resta. Punto 8: prima di `ferma` la
  sentinella fa il passaggio mancante quando è uno solo e certo (prima
  revisione o `pronta`) ([DECISIONI.md](../DECISIONI.md), 2026-11-28,
  Catena 2 e 4)

## Contesto

Ogni passaggio di un lavoro passa dal responsabile, chat per chat: l'avvio di
un pacchetto, la revisione, le correzioni, i giri successivi, la ripresa dopo
una decisione, la ripartenza dopo un merge. Con due agenti e il giro a tre
(riferimenti, lavoro, revisione; [LAVORO-IN-PARALLELO.md](../LAVORO-IN-PARALLELO.md),
"Chi fa cosa") il tempo del responsabile se ne va in messaggi da copiare.

Tecnigrafo ha adottato la catena degli agenti il 14 novembre 2026, a tappa
aperta ([ADR 0053 di Tecnigrafo](https://github.com/Matleyx78/tecnigrafo/blob/main/docs/adr/0053-catena-degli-agenti.md)),
dopo le prove nel repository usa e getta
[Matleyx78/prova-orchestratore](https://github.com/Matleyx78/prova-orchestratore)
con tutti i tipi di lavoro, compreso il **contratto annidato** che in
solid-space è successo davvero (PS5, #245, fermo sulla #286, ripartito dopo la
correzione #287). I piloti in Tecnigrafo (PT41, RT32 e PT32) hanno portato
due correzioni, già comprese qui: il lavoro esiste solo dopo `git push`, e
`scripts/ci-attendi.sh` accetta il numero di una pull request. Quell'ADR dice
che in solid-space la catena arriva con una pull request sua, con le stesse
voci: è questa.

## Decisione

1. **Avvio.** Il responsabile avvia un lavoro con **un solo commento** nella
   issue, che menziona l'agente della sua etichetta `agente:…`
   (`agente:claude-2` è Claude); per Claude il commento sceglie modello e
   impegno (`[modello: …]`, `[impegno: …]`). Codex si avvia anche dalla sua
   chat, quando serve un modello preciso. Solo il responsabile avvia gli
   agenti: il workflow di Claude parte solo per i suoi commenti, e Codex
   risponde solo a lui. Ogni catena comincia da un suo avvio o da un suo merge.
2. **Ruoli.** L'**esecutore** è l'agente dell'etichetta della issue, il
   **revisore** l'altro: il giro a tre resta com'è.
3. **Passaggi solo con gli script.** `scripts/passa.sh` (revisione,
   correzioni, analisi di una CI rossa) e `scripts/avvia.sh` scrivono la
   menzione a nome del responsabile, con un token personale a grana fine
   (secret `RELAY_PAT`) quando scrive Claude da GitHub Actions. Nessun agente
   scrive a mano la chiocciola davanti al nome di un agente. Le richieste di
   revisione e di analisi per Codex vanno nella issue collegata, perché su una
   pull request attiverebbero la sua revisione automatica.
4. **Revisione a giri.** Il verbale porta «Giro di revisione: N» e le classi
   **[bloccante]**, **[decisione]** (i buchi visibili), **[fascia]** (la regola
   della fascia, [revisione.md](../agenti/revisione.md)) e **[facoltativa]**.
   Le [bloccante] dei giri 1 e 2 tornano all'esecutore; una [decisione], o il
   giro 3 con [bloccante] aperte, fermano; altrimenti l'etichetta `pronta`. La
   fascia torna all'esecutore e non chiede un altro giro. Dal giro 2 si
   rivedono solo i commit di correzione. Che cosa blocca resta quello di
   revisione.md.
5. **Decisioni.** Chi si ferma mette `decisione` sulla issue e sulla pull
   request e lascia la riga da copiare, senza chiocciola, con il nome
   dell'agente e la issue in cui scriverla. Il responsabile decide **sempre in
   una issue**. Un revisore riprende con «Ripresa dopo la decisione (giro N)»,
   che non conta come giro.
6. **Buco nel contratto.** La issue `contratto` che l'agente apre
   ([contratto.md](../agenti/contratto.md)) porta anche `decisione` e
   l'etichetta dell'altro agente, ed è il posto della decisione. Alla ripresa
   l'altro agente scrive la correzione del contratto (**Revisione del
   contratto: da fare**, `Closes` della issue `contratto`,
   `Avvia dopo il merge: #<pacchetto>`), chi aveva il pacchetto la rivede con
   la lista di [CONTRATTI.md](../CONTRATTI.md), e al merge il pacchetto
   riparte. Nella revisione di una correzione del contratto un buco nuovo è
   una [decisione] del verbale, perché la decisione entra nella stessa pull
   request.
7. **Dopo il merge.** Il workflow *Dopo il merge* legge le righe
   `Avvia dopo il merge: #N` della pull request unita e avvia la issue N con
   il suo agente, se le issue delle sue righe «Dipende da» e «Riferimenti»
   sono chiuse; per Claude modello e impegno vengono dalla riga «Modello e
   impegno» della issue. Le scrivono i riferimenti (per il loro pacchetto) e
   le correzioni del contratto o dei riferimenti fatte dopo una decisione (per
   il pacchetto fermo).
8. **Sentinella.** Ogni 30 minuti un workflow mette `ferma` sulle catene
   avviate e senza movimento da almeno **60 minuti** (la CI con Windows e
   macOS è lenta), comprese le decisioni scritte e mai riprese, e segnala
   `AA-Presa in carico` rimasta. Lascia stare le pull request con la CI in
   corso.
9. **Niente lotti.** Con la catena ogni lavoro si avvia con il suo commento,
   anche molti nello stesso momento: il lavoro a lotti di riferimenti e
   revisioni non c'è più (`docs/agenti/lotti.md` è tolto).
10. **Ambienti.** Gli ambienti in cui gli agenti lavorano nella catena (il
    workflow di Claude, gli ambienti di Codex e i loro script di preparazione)
    li decide il responsabile ([comandi.md](../agenti/comandi.md), "Gli
    ambienti della catena").
11. **Dove sono le regole.** Le regole per gli agenti sono in
    [docs/agenti/catena.md](../agenti/catena.md), con due frasi nel nucleo di
    AGENTS.md (la chiocciola e il passaggio prima di terminare) e una riga
    nella sua tabella; i file del lavoro rimandano a catena.md dove la catena
    cambia il modo di fermarsi. Le frasi e la preparazione per il responsabile
    sono nella guida comune, in Tecnigrafo
    ([GUIDA-RESPONSABILE.md](https://github.com/Matleyx78/tecnigrafo/blob/main/docs/GUIDA-RESPONSABILE.md),
    A2a). Gli script della catena e i loro test vengono da Tecnigrafo, con il
    commit di origine nell'intestazione; i test girano nella CI.
12. **Quando vale.** Dal merge di questa pull request, **a tappa aperta** (la
    0.3), in via eccezionale rispetto alla regola che il metodo cambia solo
    nella pausa fra le tappe ([DECISIONI.md](../DECISIONI.md), 2026-11-07),
    come in Tecnigrafo. Ogni agente legge l'AGENTS.md del suo ramo, quindi una
    pull request nata prima va aggiornata su `main` prima di entrare nella
    catena.

## Alternative considerate

- **Aspettare la pausa dopo la 0.3**: scartata per lo stesso motivo di
  Tecnigrafo, il guadagno di tempo vale già per il resto della tappa; e le
  regole dei due progetti restano allineate.
- **Un orchestratore che avvia da solo le issue pronte**: scartato, come in
  Tecnigrafo, perché solo il responsabile avvia gli agenti.
- **Issue `contratto` a parte per ogni buco nuovo trovato rivedendo una
  correzione del contratto**, come dice CONTRATTI.md fuori dalla catena:
  scartata nella catena, perché quel buco si decide prima del merge della
  stessa pull request, e la [decisione] del verbale ferma la catena nel posto
  giusto.

## Conseguenze

- Il responsabile interviene solo per avvii, decisioni, lettura finale e
  merge; le etichette `pronta`, `decisione` e `ferma` gli dicono dove.
- I rallentamenti veri sono la quota di Codex (cinque ore e settimanale),
  coperta dalla sentinella, la coda di GitHub Actions e la preparazione
  dell'ambiente legacy di Codex senza cache, a ogni avvio.
- Codex avviato da una menzione usa il suo modello predefinito, con impegno
  non noto: per un modello preciso (le revisioni del contratto chiedono la
  fascia forte) si avvia dalla chat, e la catena prosegue lo stesso.
- Il workflow di Claude gira fino a cinque ore, perché aspetta la CI; consuma
  il piano Max, non l'API.
- Serve una preparazione una volta per repository: app di Claude, secret
  `CLAUDE_CODE_OAUTH_TOKEN` e `RELAY_PAT`, un ambiente legacy di Codex per le
  menzioni (guida, A2a).
