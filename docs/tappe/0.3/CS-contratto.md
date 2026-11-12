# [0.3-CS] Revisione del contratto

- **Tappa:** 0.3, per Tecnigrafo 0.8 — [piano](../0.3-regioni-ed-ellisse.md)
- **Issue:** [#222](https://github.com/Matleyx78/solid-space/issues/222)
- **Agente:** codex
- **Difficoltà:** impegnativa
- **Modello e impegno:** forte, alto
- **Dipende da:** valutazioni #218, #220, #219 e #221, già unite
- **Contratto:** `region.hpp`, `boundary.hpp`, `clipping.hpp`,
  `elementary.hpp`, `ellipse.hpp`
- **Riferimenti:** i casi di verifica spettano ai pacchetti della tappa

La issue contiene il verbale della revisione delle cinque intestazioni,
svolta secondo [CONTRATTI.md](../../CONTRATTI.md). Le decisioni del responsabile
nelle issue #223–#230 autorizzano la correzione dei commenti nelle intestazioni
di regione, contorno, taglio e funzioni elementari e dei relativi
[ADR 0014](../../adr/0014-contorno-e-isole-di-una-regione.md),
[0015](../../adr/0015-parti-di-una-curva-dentro-una-regione.md) e
[0016](../../adr/0016-tangente-arcocoseno-e-lunghezza-deterministici.md).

La decisione nella [#232](https://github.com/Matleyx78/solid-space/issues/232)
e il [via libera sul passo 6](https://github.com/Matleyx78/solid-space/pull/231#issuecomment-6815626390)
autorizzano anche le correzioni di `detectBoundary` e dell’ADR 0014 emerse
nella revisione della pull request #231.

## Modifiche al piano

- **2026-11-12 — Correzioni dopo la revisione #222.** Le schede di lavoro e
  di riferimenti della 0.3 che realizzano o usano il contratto dipendono da
  CS. Per il contorno recepiscono i collegamenti alla scala delle curve,
  il primo incontro verso l'esterno, le isole a contatto con il contorno
  e le origini dei giri circolari (#223–#228). Per il taglio recepiscono
  l'indipendenza delle liste (#229). Per le funzioni elementari recepiscono
  il caso del vettore nullo nella composizione di `acos` e `hypot` (#230).
  Voce da riportare nel piano della tappa alla sua pubblicazione.

- **2026-11-12 — Correzioni dopo la revisione della #231.** Nei passi 2, 3
  e 4 di `detectBoundary` vale la M della curva data, invariata dal
  prolungamento; nel passo 4 contano anche le coordinate del punto scelto
  (#232). La frase finale del passo 6 esclude i pezzi interni solo negli
  altri gruppi, come autorizzato nella #231 per applicare la #227.
  Le schede di lavoro e di riferimenti che usano il contratto recepiscono
  queste regole. Voce da riportare nel piano della tappa alla pubblicazione.
