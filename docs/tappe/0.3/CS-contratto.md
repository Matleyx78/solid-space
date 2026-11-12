# [0.3-CS] Revisione del contratto

- **Tappa:** 0.3, in preparazione, per Tecnigrafo 0.8
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

## Modifiche al piano

- **2026-11-12 — Correzioni dopo la revisione #222.** Le schede di lavoro e
  di riferimenti della 0.3 che realizzano o usano il contratto dipendono da
  CS. Per il contorno recepiscono i collegamenti alla scala delle curve,
  il primo incontro verso l'esterno, le isole a contatto con il contorno
  e le origini dei giri circolari (#223–#228). Per il taglio recepiscono
  l'indipendenza delle liste (#229). Per le funzioni elementari recepiscono
  il caso del vettore nullo nella composizione di `acos` e `hypot` (#230).
  Voce da riportare nel piano della tappa alla sua pubblicazione.
