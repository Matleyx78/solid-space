# Il contratto

Si legge insieme al nucleo, [AGENTS.md](../../AGENTS.md), quando il lavoro tocca
un file in `*/include/solid_space/` o [docs/CONTRATTI.md](../CONTRATTI.md),
quando la scheda ha la riga **Contratto:** o cita intestazioni pubbliche in
**Da leggere**, oppure quando la issue ha l'etichetta `contratto`. Le sezioni
portano i titoli che avevano in AGENTS.md.

## Il contratto

Le intestazioni pubbliche del contratto (ADR 0007, punto 15) si realizzano senza
cambiarle, commenti compresi; le cambia solo un pacchetto la cui scheda lo
autorizza, dopo una decisione del responsabile ([docs/CONTRATTI.md](../CONTRATTI.md)).

**Prima di scrivere codice o casi**, rileggi le funzioni che il pacchetto
realizza o usa con la lista di controllo di [docs/CONTRATTI.md](../CONTRATTI.md).
Se un punto non è determinato dall'intestazione, dall'ADR o dalla scheda (per
esempio che cosa succede a un punto costruito oltre `kMaxCoordinate`), o se due
di questi si contraddicono, ti fermi come dice AGENTS.md, "Quando fermarsi"
(«Un buco nel contratto», e «Buchi visibili e no» per sapere quando la sosta
vale). Non scegliere tu una risposta, nemmeno segnalandola: il contratto lo
decide il responsabile, e Tecnigrafo ci ha già scritto sopra.

La issue del buco:

- ha le etichette `contratto` e `tappa-X.Y`;
- è intitolata `[X.Y-PN] <intestazione>: <funzione>: <punto>` (o `[X.Y-RN]`);
- dice che cosa manca, con un esempio con i dati, le scelte possibili e quella
  che consigli.

La citi nella issue del tuo pacchetto, togli `AA-Presa in carico` e termini la
sessione. Il pacchetto si riprende dopo la decisione. Se trovi il buco a lavoro
già avanzato, vale lo stesso: pubblica il ramo senza pull request e scrivine il
nome nella issue.

**Se la tua pull request cambia un'intestazione del contratto** (anche solo un
commento, per esempio applicando una decisione), metti in testa alla
descrizione la riga **Revisione del contratto: da fare**, con le intestazioni e
le funzioni che cambia: prima del merge la rivede con la lista un agente diverso
da te ([docs/CONTRATTI.md](../CONTRATTI.md), «Le correzioni del contratto»).
Se ti chiedono di rivedere una pull request così, segui
[revisione.md](revisione.md).

Unica eccezione: la fascia che il revisore ti segnala per un caso che sta entro
un errore dichiarato dal confine di `kMaxCoordinate` o da un'altra soglia del
contratto la scrivi nella stessa pull request, nel commento dell'intestazione
(ADR 0013, punto 4), senza un altro giro di revisione
([revisione.md](revisione.md), «La fascia»).

## Prima della pull request

Viene dal punto 1 di AGENTS.md, "Quando fermarsi". Prima di aprire la pull
request, aggiorna `main` e controlla che le intestazioni del contratto che il
pacchetto realizza o usa non siano cambiate da quando hai cominciato:

```sh
git fetch origin main
git diff "$(git merge-base origin/main HEAD)" origin/main -- <intestazioni>
```

Se sono cambiate, leggi la decisione (in [docs/DECISIONI.md](../DECISIONI.md))
e adegua il pacchetto partendo da `main` aggiornato; se il cambiamento
contraddice il lavoro fatto in un modo che non sai risolvere, fermati come per
un buco nel contratto.

Se la pull request cambia un'intestazione del contratto, scrivilo in un
commento nelle issue dei pacchetti in lavorazione (con `AA-Presa in carico`)
che la realizzano o la usano.
