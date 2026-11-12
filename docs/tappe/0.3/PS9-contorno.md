# [0.3-PS9] `geom2d`: contorno e isole da un punto

- **Tappa:** 0.3 — [piano](../0.3-regioni-ed-ellisse.md)
- **Issue:** #257
- **Difficoltà:** media
- **Agente:** claude
- **Modello e impegno:** medio; impegno alto. La revisione di Codex è mirata,
  in fascia forte, sulla regola (collegamenti, prolungamenti, punto sopra una
  curva) e sulla costruzione dei loop
- **Riferimenti:** issue `[0.3-RS9a]` (#253) e `[0.3-RS9b]` (#254), agente codex,
  fascia forte; `[0.3-RS9c]` (#255) e `[0.3-RS9d]` (#256), agente codex, fascia
  leggera (RS9d: `autocad-retini.dxf`)
- **Contratto:** `geom2d/include/solid_space/geom2d/boundary.hpp`
  (`detectBoundary`, `BoundaryOutcome`, `OpenEnd`, `DetectedBoundary`); usa
  `region.hpp` (`RegionLoop`, `LoopPieceOrigin`, `kRelativeLoopError`,
  `SkippedInput`), `curves.hpp` e `robust/tolerance.hpp`
- **Dipende da:** PS6 (#246); RS9a (#253), RS9b (#254), RS9c (#255), RS9d (#256). La
  revisione del contratto `[0.3-CS]` (#222) e PS2 arrivano attraverso PS6
- **Sblocca in Tecnigrafo:** PT43 ([Matleyx78/tecnigrafo#740](https://github.com/Matleyx78/tecnigrafo/issues/740)), con PS10: il
  contorno rilevato da un punto; con PS5 e PS10, PT44 (`-HATCH`)
- **Componente:** `geom2d`
- **Da leggere:** [ADR 0014](../../adr/0014-contorno-e-isole-di-una-regione.md),
  con le due righe **Corretto:** (#223–#228, #232); [ADR 0009](../../adr/0009-regola-delle-tolleranze-del-piano.md)
  (la regola cambia i dati prima delle decisioni); [ADR 0013](../../adr/0013-esatto-dove-serve.md),
  punto 4 (la fascia degli archi); `boundary.hpp` e `region.hpp`; le schede
  [PS2](PS2-intorno-e-parita.md) e [PS6](PS6-arrangiamento.md) e le loro
  intestazioni; gli interni della 0.2 `carriers.hpp`, `positions.hpp` e
  `curve_meaning.hpp`; `robust/tolerance.hpp`

## File

- `geom2d/src/boundary.cpp` (nuovo); se serve, un'intestazione interna nuova
  tua per i passi 1–4 (per esempio `boundary_rule.hpp`)
- `tests/geom2d/boundary_test.cpp` (nuovo)
- `tests/corpus/esegui/op_boundary.cpp` (nuovo)
- `tests/corpus/in-attesa/`: cancellare `0.3-RS9a.txt`, `0.3-RS9b.txt`,
  `0.3-RS9c.txt` e `0.3-RS9d.txt`

Nessun `CMakeLists.txt`. `nestContours`, nella stessa intestazione, è di PS10,
in un file suo: questo pacchetto non la realizza. Se il pacchetto supera la
misura, i passi 1–4 (la regola) diventano un pacchetto a sé, con una riga in
«Modifiche al piano» decisa dal responsabile.

## Cosa fare

Realizzare, senza cambiarla, `detectBoundary` di `boundary.hpp`:

```cpp
[[nodiscard]] Result<DetectedBoundary> detectBoundary(Point2 picked, std::span<const Curve> curves,
                                                      const robust::TolerancePolicy& policy);
```

Con i sette passi esatti dell'intestazione, come li hanno corretti le decisioni
#223–#228 e #232:

1. **Dati:** gli scarti in `skipped` (input i + 1); l'arco di un giro intero
   diventa il suo cerchio, gli altri archi uno o due pezzi di polilinea entro
   `kRelativeLoopError / 2`; i pezzi con gli estremi arrotondati coincidenti
   tolti senza diagnosi;
2. **collegamenti** fra estremi distinti entro epsilon delle loro curve, con
   la M della curva data;
3. **prolungamenti** fino al primo punto comune sull'estensione verso
   l'esterno, tangenze comprese, se entro epsilon per la curva prolungata e
   una curva incontrata lì; la M non cambia;
4. **punto sopra una curva** (anche prolungata) o un collegamento, entro
   epsilon con M che conta anche il punto: `onCurve` con `nearCurve`;
5. **arrangiamento** con `arrangement.hpp` (PS6); faccia illimitata:
   `notEnclosed`, con gli estremi liberi del primo gruppo con estremi liberi
   lungo la semiretta verso +x;
6. **loop:** il contorno, le altre isole della faccia (anche se toccano il
   contorno, #227), il bordo esterno degli altri gruppi dentro il contorno;
7. **profondità.**

Poi la costruzione dei `RegionLoop`: verso antiorario; un giro intero come
`Circle` solo se tutti i suoi pezzi sono accreditati allo stesso `Circle` o
`Arc` intero (#228); altrimenti una polilinea chiusa dal vertice minore in
ordine lessicografico esatto, con i pezzi curvi oltre mezzo giro divisi in
due di angolo uguale, gli estremi dei dati copiati, gli altri vertici entro
`kRelativePointError` e ogni pezzo entro `kRelativeLoopError`; le origini
accanto a ogni pezzo; l'ordine dei loop dell'intestazione.

La M, l'epsilon e la scala di ogni confronto sono quelli dell'intestazione; nei
punti in cui un arco convertito sfiora un'altra curva o il punto le risposte
sono libere (fascia, ADR 0013, punto 4).

### Casi di verifica (per RS9a, RS9b, RS9c e RS9d)

| Operazione | `ingresso` | `atteso` |
|---|---|---|
| `detectBoundary` | `punto`, `curve` (elenco di curve; le polilinee come pezzi), `unita_in_metri` | `esito` (`enclosed`, `onCurve`, `notEnclosed`); `loop`: elenco di `{"loop": {"cerchio": …} o {"vertici": [[x, y, bulge], …], "chiusa": true}, "origini": [{"fonte": "curve" o "link", "curva": intero, "altra_curva": intero, "invertito": booleano}], "profondita": intero}`; `curva_vicina`: intero; `estremi_liberi`: elenco di `{"curva": intero, "all_inizio": booleano, "punto": punto}`; `scartate`: elenco di `{"indice": intero, "diagnosi": {...}}`; oppure `diagnosi` (`dato` 0) |

La `TolerancePolicy` si costruisce con `forUnit(unita_in_metri)`, come in
`op_coincidence.cpp`. Esito, numero e ordine dei loop e dei pezzi, origini,
profondità, indici e booleani sono esatti; i vertici con tolleranza
`kRelativePointError · max(M, |p|)` (gli estremi dei dati esatti), i bulge con
una tolleranza che chi scrive il caso ricava da `kRelativeLoopError` e scrive
in `provenienza`. Nessun caso fissa una risposta nella fascia degli archi.

**RS9a** (fascia forte), la regola, alle due scale dell'ADR 0009:

- due estremi a distanza uguale a epsilon e un `double` oltre; i dati delle
  decisioni #223 (scala dei collegamenti con la M della curva data) e #232 (M
  invariata dal prolungamento, coordinate del punto scelto nel passo 4);
- un estremo tagliato da `TRIM` a un arrotondamento dalla curva che taglia; un
  arco raccordato a due linee (`FILLET`); un primo incontro tangente entro la
  tolleranza (si prolunga) e uno oltre la tolleranza su una curva vicina
  all'estremo (resta libero), con i dati di #224–#226;
- il punto scelto a distanza uguale a epsilon da una curva e un `double` oltre
  (`onCurve` o no).

**RS9b** (fascia forte), la topologia:

- vertici con quattro o più pezzi, tangenze interne ed esterne nello stesso
  vertice, tratti sovrapposti da più curve (attribuiti alla curva di indice
  minore), facce con il bordo che tocca se stesso;
- isole che toccano il contorno (#227); gruppi di curve dentro un'isola; curve
  aperte dentro la regione; un cerchio con un diametro (l'isola è il cerchio);
- un giro intero con origini miste, restituito come polilinea (#228), e uno
  accreditato a un solo cerchio, restituito come `Circle`.

**RS9c** (fascia leggera):

- #214, casi 1, 1 bis, 2, 3a, 3b, 3c, 4 (spazio entro la tolleranza e più largo,
  con gli estremi liberi) e 5, alle due scale;
- `notEnclosed` senza estremi liberi (punto fuori da ogni contorno chiuso);
  curve scartate in mezzo alle buone; punto non valido o fuori dominio.

**RS9d** (fascia leggera), dopo RS5c, che porta `autocad-retini.dxf` in
`sorgenti.json` e in `esterni/`: con le curve del file (il rettangolo con il
cerchio, le isole annidate) e un punto interno, i loop del contratto,
ricavati con l'aritmetica esatta; i contorni che AutoCAD ha salvato nei
`HATCH` fanno da confronto (stessi pezzi, stessi vertici entro la
tolleranza), e dove AutoCAD fa diversamente da un limite dell'ADR 0014
(punto 10) il caso segue il contratto e la `descrizione` lo dice.

Valori attesi: decisioni con `fractions` di Python e numeri della forma
a + b·√d; vertici con `decimal`.

## Regole della tappa che valgono qui

- **Contratto:** `boundary.hpp` e `region.hpp` non cambiano, commenti compresi.
  Se qualcosa manca o non torna, fermati come dice AGENTS.md.
- **Gli interni** della 0.2, di PS2 e di PS6 si usano senza cambiarli.
- **Determinismo** (ADR 0008, punto 5): calcolo nei `.cpp`, niente funzioni
  trigonometriche di `<cmath>`.
- File comuni: nessuno; cancelli solo i file dei tuoi riferimenti.

## Test richiesti

- `boundary_test.cpp`, sulle proprietà dell'ADR 0014 («Verifica»), su dati
  generati in modo deterministico: permutando le curve i loop restano gli stessi
  entro gli errori e le origini si permutano; ogni loop di profondità k è
  racchiuso da esattamente k altri loop; con `enclosed`, il risultato non cambia
  aggiungendo fuori dal contorno curve che non toccano le altre; ogni vertice e
  pezzo entro il suo errore dal valore esatto.
- I casi di RS9a, RS9b, RS9c e RS9d passano.

## Fatto quando

- `detectBoundary` è realizzata come dichiara `boundary.hpp`.
- I casi di `[0.3-RS9a]`–`[0.3-RS9d]` passano e i loro file sono cancellati da
  `in-attesa/`.
- Il job del determinismo è verde.
- La CI è verde.
