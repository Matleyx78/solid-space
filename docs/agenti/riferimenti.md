# Pacchetto di riferimenti

Si legge insieme al nucleo, [AGENTS.md](../../AGENTS.md), quando la issue ha
l'etichetta `riferimenti` o il titolo `[X.Y-RN]`, oppure quando il lavoro tocca
`tests/corpus/`. Le etichette degli agenti sono in [pacchetto.md](pacchetto.md),
«Le etichette degli agenti». Le sezioni portano i titoli che avevano in AGENTS.md.

Per "il prossimo pacchetto di riferimenti" valgono le condizioni 1–3 e 5 di
[pacchetto.md](pacchetto.md) ("Se ti chiedono di prendere il prossimo
pacchetto"), sulle issue con l'etichetta `riferimenti`; per prenderli di fila,
[lotti.md](lotti.md).

## Robustezza e verifica

Un pacchetto che cambia il comportamento geometrico è finito quando i test
confermano casi di verifica scritti **prima** da un agente o da una persona
**diversi** da chi scrive il codice ([ADR 0004](../adr/0004-robustezza-e-verifica.md)).
Chi scrive il codice segue [pacchetto.md](pacchetto.md) ("In un pacchetto di
lavoro").

### Se scrivi i casi di riferimento

- Ricava i valori attesi da calcoli a mano, da aritmetica esatta o da file
  pubblici. **Mai eseguendo solid-space**: registreresti il suo comportamento,
  errori compresi.
- Per ogni file preso dal web registra in `sorgenti.json` indirizzo, versione,
  hash SHA-256 e licenza, compatibile con la GPLv3 e ridistribuibile. Nel dubbio,
  non usare il file e scrivilo nella pull request.
- Aggiungi i casi limite: valori nulli, punti coincidenti o quasi allineati,
  facce tangenti, spessori minimi.
- Se il contratto non determina il valore atteso di un caso, non sceglierlo tu:
  fermati come dice AGENTS.md, "Quando fermarsi" («Un buco nel contratto»).
- Una pull request che tocca solo casi di riferimento (o anche documenti e
  commenti) non si compila in locale: la verifica la fa la CI
  ([DECISIONI.md](../DECISIONI.md), 2026-11-03).
