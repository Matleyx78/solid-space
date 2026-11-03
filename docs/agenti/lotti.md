# Lavoro a lotti

Si legge insieme al nucleo, [AGENTS.md](../../AGENTS.md), quando ti chiedono
«Prendi i pacchetti di riferimenti pronti» o «Rivedi le pull request aperte». Con
il primo leggi anche [riferimenti.md](riferimenti.md), con il secondo
[revisione.md](revisione.md). La sezione porta il titolo che aveva in AGENTS.md.

## Lavoro a lotti

Due eccezioni alla regola "un pacchetto per sessione":

- **"Prendi i pacchetti di riferimenti pronti"**: uno dopo l'altro, nell'ordine
  delle condizioni 1–3 e 5 ([pacchetto.md](pacchetto.md), "Se ti chiedono di
  prendere il prossimo pacchetto"), ciascuno con il suo ramo partito da `main`
  aggiornato e la sua pull request portata alla CI verde; alla fine un solo
  riepilogo con tutte le pull request.
- **"Rivedi le pull request aperte"**: una alla volta, tutte quelle senza
  `AA-Presa in carico` scritte da un modello diverso dal tuo, prese in carico come
  dice AGENTS.md, "Per prima cosa: prendi in carico", con un commento di
  revisione. Non approvare e non unire. Quelle con **Revisione del contratto: da
  fare** si rivedono con la lista ([docs/CONTRATTI.md](../CONTRATTI.md), «Le
  correzioni del contratto»).

Se in un lotto ti fermi per uno dei motivi di AGENTS.md, "Quando fermarsi",
scrivilo nella pull request e passa al pacchetto successivo; se ti fermi per il
contratto («Un buco nel contratto», nella stessa sezione), apri la issue e passa
al successivo.
