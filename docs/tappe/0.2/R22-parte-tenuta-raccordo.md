# [0.2-R22] Riferimenti: parte di ciascuna curva tenuta dal raccordo

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** da aprire
- **Agente:** codex
- **Difficoltà:** impegnativa
- **Modello e impegno:** forte; impegno alto
- **Riferimenti del pacchetto:** [P22](P22-parte-tenuta-raccordo.md)
- **Contratto:** `geom2d/include/solid_space/geom2d/tangency.hpp`
  (`KeptPart`, `Fillet::keepFirst` e `Fillet::keepSecond`, regola 5 di
  `fillet`)
- **Dipende da:** P1 (#34)
- **Sblocca in Tecnigrafo:** Matleyx78/tecnigrafo#367 (P19, raccordi), che
  taglia e prolunga le curve con la parte tenuta
- **Da leggere:** [ADR 0011](../../adr/0011-raccordi-e-tangenze-nel-piano.md)
  (riga **Corretto:** del 2026-11-04); in `tangency.hpp` il commento di
  `KeptPart`, `TangencyOutcome::ambiguousPick` e le regole 1–5 di `fillet`;
  la decisione #184 in [DECISIONI](../../DECISIONI.md); il
  [README dell'archivio](../../../tests/corpus/README.md) e AGENTS.md

Non realizza P22 e non modifica il codice. I casi di `fillet` già nell'archivio
non si toccano: il confronto controlla solo le chiavi scritte, quindi restano
validi senza le chiavi nuove.

## Vocabolario

L'operazione `fillet` esiste già (R16a–R16c); si aggiungono due chiavi di
`atteso`, scritte con il nome del valore di `KeptPart`:

| Operazione | `ingresso` | `atteso`, in più |
|---|---|---|
| `fillet` | come in R16a–R16c | `parte_prima`, `parte_seconda`: `fromStart` oppure `toEnd` (solo con `esito` `found`) |

## Cosa verificare

- **Gli esempi della #184.** L1 da (0,0) a (30,0), scelta in (5,0); L2 da
  (0,0) a (0,30), scelta in (0,5); raggio 10: `toEnd` per tutte e due (L1 da
  (10,0) a (30,0)). Arco di centro (230,0), raggio 10, da 0 a π, scelto in
  (238.6603,5), e linea da (215,−5) a (250,−5), scelta in (245,−5), raggio 3
  (caso e di `autocad-raccordi.dxf`): `toEnd` per tutte e due (l'arco da
  circa 351.15° a 180°, cioè si prolunga l'inizio; la linea dal punto di
  tangenza, x ≈ 242.845, a (250,−5)).
- **Regola 3, punti `inside`**, per ogni coppia di tipi (segmento, arco,
  cerchio, segmento di polilinea dritto e curvo): i due valori, nei due versi
  di ciascuna curva e con le curve scambiate (la stessa parte geometrica:
  `fromStart` e `toEnd` si scambiano invertendo il verso; `parte_prima` e
  `parte_seconda` scambiando le curve).
- **Punti fuori dalla curva** (regola 2 della #184): segmento con il punto di
  tangenza `beforeStart` e `afterEnd`; arco con il punto `afterEnd` vicino
  all'inizio e vicino alla fine, nei due versi dell'arco. Anche i casi in cui
  la parte tenuta non contiene altro della curva data (un segmento corto tutto
  tra lo spigolo e il punto di tangenza; il punto di tangenza esattamente
  all'estremo tenuto).
- **Regola 1, semicerchio:** le due parti, anche con la seconda linea più
  corta o spostata oltre il semicerchio.
- **Regola 2, raggio zero, e punti di tangenza esattamente coincidenti:** la
  parte dalla parte del punto indicato rispetto allo spigolo, per segmenti e
  per curve circolari; il punto indicato esattamente sulla normale della sua
  curva allo spigolo (`ambiguousPick`), e a un ulp da essa da ciascuna parte;
  `ambiguousPick` dopo una diagnosi di dominio dello spigolo (vince
  `outOfDomain`, dato 2).
- **Cerchio intero:** il valore nel verso antiorario del cerchio.

## File

Solo `tests/corpus/`: casi nuovi in `casi/geom2d/` e
`in-attesa/0.2-R22.txt` (i casi nuovi, con la issue di P22).

## Valori attesi e fatto quando

Da aritmetica esatta, mai eseguendo solid-space: il segno di dot(t, a) o di
dot(pick − P, t) della regola 5, con `fractions` di Python e confronti tra
numeri algebrici (elevamenti al quadrato). Nel `procedimento`, per ogni caso,
la direzione della curva nel punto di tangenza, il verso dell'arco e il segno
che decide. Punti, archi e bulge, se si scrivono, con la tolleranza del loro
errore dichiarato. Se il contratto non determina un caso, fermarsi secondo
AGENTS.md. I casi coprono le voci sopra, passano la validazione e sono
pubblicati separatamente dal lavoro P22, con CI verde.
