# [0.2-R19] Riferimenti: bulge e dominio della curva parallela

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** #129
- **Agente:** codex
- **Difficoltà:** impegnativa
- **Modello e impegno:** forte; impegno alto
- **Riferimenti del pacchetto:** [P19](P19-bulge-parallela.md), #130
- **Contratto:** `geom2d/include/solid_space/geom2d/offset.hpp`, regole generali
  di `curves.hpp`
- **Dipende da:** P1 (#34), C (#133, con le decisioni unite)
- **Da leggere:** [ADR 0012](../../adr/0012-curva-parallela-nel-piano.md), punti
  3, 5–7 e 9; [ADR 0013](../../adr/0013-esatto-dove-serve.md), punti 1, 4 e 5;
  decisioni #127 e #132, [CONTRATTI.md](../../CONTRATTI.md);
  [README dell'archivio](../../../tests/corpus/README.md) e AGENTS.md

Questa scheda riprende l'ambito della #129 secondo le decisioni #132, #148,
#149, #153 e #154, ridotto dall'ADR 0013 (2026-11-02): i vertici non sono più
il `double` più vicino, e gli esiti che dipendono solo dall'arrotondamento
stanno in una fascia che nessun caso impone. Non realizza P19 e non modifica il
codice.

## Cosa verificare

Solo esiti e valori che il contratto determina:

- **Bulge ricostruito**, entro la tolleranza del bulge (4E/corda, con
  E = 2^-46 · max(M, |p|) sui punti dell'arco esatto, come nei casi della
  pull request #163): pezzi curvi di `offsetCurve` con estremi lontani tra
  loro; in `offsetPolyline` pezzi curvi tagliati o prolungati e archi di
  giunzione; semicirconferenze esatte con bulge ±1. I vertici si confrontano
  entro `kRelativePointError`.
- **Dominio sui valori esatti (#149)**, K = 2^50: segmento x = K−1/8 con
  d = −1/4 (`outOfDomain`, dato 1), stessa polilinea (`beyondDomain`),
  cerchio r = K con d = −1/8 (`outOfDomain`, dato 1).
- **#153**, K = 2^50, t = 3·2^-34: segmento (K, 0)–(K−1/8, t), d = −2^26:
  `offsetCurve` dà `outOfDomain`, dato 1, prima di ogni `failed`.
- **Ordine dei controlli finali** di `offsetPolyline`: un risultato con un
  vertice esatto oltre K e anche un'autointersezione dà `beyondDomain`.

Non stanno nel corpus, perché il loro esito è in una fascia di `offset.hpp`
(ADR 0013, punti 4 e 5), e li prova P19 con test di unità:

- estremi esatti a meno di 2 · `kRelativePointError` · max(M, |p|) per
  coordinata: i casi #148, la polilinea della #153, la #154, gli archi con
  estremi arrotondati coincidenti;
- archi quasi interi con la corda di pochi ulp: il caso della #127 e quello
  della #132 (`notRepresentable`);
- con E = `kRelativeOffsetError` · max(M, |p|): il punto m di un pezzo curvo
  quando un punto dell'arco esatto ha una coordinata oltre K − E in valore
  assoluto (`outOfDomain`, dato 1, in `offsetCurve`; `beyondDomain` in
  `offsetPolyline`); il risultato esatto entro E
  dall'ingresso (`meetsOriginal`); due pezzi esatti non consecutivi entro 2E
  l'uno dall'altro, o due consecutivi entro 2E lontano dal vertice comune,
  oltre la coincidenza di `policy` (`selfIntersection`).

## File

Solo `tests/corpus/`: casi nuovi in `casi/geom2d/` e `in-attesa/0.2-R19.txt`
(casi nuovi con #130).

## Valori attesi e fatto quando

Calcoli a mano o aritmetica esatta e ad alta precisione, mai eseguendo
solid-space. Documentare nel `procedimento` il calcolo del bulge, della sua
tolleranza e delle risposte discrete, e perché i dati stanno fuori dalle fasce;
se il contratto non determina un caso, fermarsi secondo AGENTS.md. I casi
coprono le voci sopra, passano la validazione e sono pubblicati separatamente
dal lavoro P19, con CI verde.
