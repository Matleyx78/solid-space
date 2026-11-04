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

L'operazione `fillet` esiste già (R16a–R16c). Si aggiunge l'esito
`radiusTooLarge`, senza geometria, e due chiavi di `atteso` per `found`:

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
- **Tangenze fuori curva.** Provare il prolungamento di ciascun estremo
  su rette e archi, e T esattamente all'estremo conservato (parte puntiforme).
- **Semicerchi (#186).** L1 (0,0)→(10,0), L2 (0,2)→(10,2), scelte
  (5,0) e (5,2), raggio 1: la regola 1 sceglie l'estremo x=10;
  invertendo L1 sceglie x=0. Per la regola 4, segmento
  (−10,0)→(10,0), cerchio di centro (0,3) e raggio 1, scelte (0,0) e
  (0,2), raggio di raccordo 1: le tangenze sono (0,0) e (0,2). L'arco
  antiorario è la metà destra, con `fromStart` sul segmento. Scambiando
  le curve è la metà sinistra, con `toEnd` sul segmento.
- **Ritorno oltre l'estremo conservato (#187).** L1 (2,0)→(5,0), L2
  (0,0)→(0,30), scelte (3,0) e (0,20), raggio 10. La tangenza
  (10,0) è `afterEnd`; `toEnd` richiederebbe un ritorno verso (5,0).
  Esito `radiusTooLarge` senza geometria, anche invertendo L1.
- **Senza arco di raccordo.** Per le rette provare il clic sulla
  normale (`ambiguousPick`) e a un ulp da ciascun lato. Una diagnosi
  di dominio dello spigolo ha precedenza.
- **Archi a raggio zero (#188).** Arco unitario antiorario da (1,0)
  a (0,1), retta (−1,−2)→(−1,2), scelte (1,0) e (−1,1). Lo
  spigolo (−1,0) è fuori dall'arco. Entrambe le parti contengono
  il clic: la fine si prolunga di 90°, l'inizio di 180°. Atteso
  `fromStart`; invertendo l'arco, `toEnd` con la stessa geometria.
  In parità esatta, con arco antiorario da (0,−1) a (0,1) e gli
  stessi spigolo e clic, atteso `ambiguousPick`. Provare anche un
  arco lungo con il clic oltre l'antipodo dello spigolo e dentro
  l'arco dato.
- **Cerchio intero:** il valore nel verso antiorario del cerchio.

## File

Solo `tests/corpus/`: casi nuovi in `casi/geom2d/` e
`in-attesa/0.2-R22.txt` (i casi nuovi, con la issue di P22).

## Valori attesi e fatto quando

Da aritmetica esatta, mai eseguendo solid-space: i segni e i confronti
angolari della regola 5, con `fractions` di Python e confronti tra
numeri algebrici (elevamenti al quadrato). Nel `procedimento`, per ogni caso,
la direzione della curva nel punto di tangenza, il verso dell'arco e il confronto
che decide. Punti, archi e bulge, se si scrivono, con la tolleranza del loro
errore dichiarato. Se il contratto non determina un caso, fermarsi secondo
AGENTS.md. I casi coprono le voci sopra, passano la validazione e sono
pubblicati separatamente dal lavoro P22, con CI verde.
