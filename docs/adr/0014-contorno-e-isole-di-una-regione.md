# ADR 0014: Contorno chiuso e isole di una regione da un insieme di curve del piano

- **Stato:** Accettato
- **Data:** 2026-11-11
- **Corretto:** 2026-11-12 — scala dei collegamenti, prolungamenti, isole e
  origine dei cerchi interi, decisioni #223–#228 della revisione #222.
- **Corretto:** 2026-11-12 — M della curva data nei passi 2, 3 e 4,
  invariata dal prolungamento (#232); frase del passo 6 limitata agli altri
  gruppi, come autorizzato nella #231 per applicare la #227.
- **Richiesta:** #214 — da Tecnigrafo: tappa 0.8, Matleyx78/tecnigrafo#661
- **Esito:** 3 pezzo nostro
- **Tappa:** 0.3, per Tecnigrafo 0.8 (scheda da scrivere all'apertura)

## Richiesta

Il comando dei retini della tappa 0.8 di Tecnigrafo deve trovare, intorno a un
punto indicato dall'utente, il contorno chiuso più piccolo fra le curve del
disegno e le isole che contiene, e dire quando la regione non è chiusa e dove.
Con i contorni scelti dall'utente (cerchi e polilinee chiuse) deve sapere quali
stanno dentro quali e a che profondità, per gli stili delle isole. Gli stessi
risultati sui tre sistemi, senza OCCT, con qualche migliaio di curve candidate
in un tempo che non si nota a un clic. Il testo completo è nella issue #214;
in Tecnigrafo il bisogno nasce dall'ADR 0048, punto 5.

## Valutazione

- **Spetta a solid-space.** Quali curve chiudono una regione, quali isole
  contiene e quali contorni stanno dentro quali sono decisioni geometriche del
  piano, compito di `geom2d` ([ADR 0008](0008-componente-geom2d.md)); l'ADR
  0048 di Tecnigrafo le affida qui e tiene per sé la scelta delle curve
  candidate, le unità, la traduzione delle entità, i motivi e i tratteggi.
- **Esiti 1 e 2 esclusi da un vincolo già preso:** `core` di Tecnigrafo compila
  senza OCCT (ADR 0030 di Tecnigrafo, punto 5;
  [ADR 0007](0007-tappe-su-richiesta.md), punto 8).
- **Esito 3**, in `geom2d`, sopra le decisioni esatte dell'[ADR
  0010](0010-posizione-e-intersezioni-nel-piano.md) (intersezioni, posizioni
  lungo le curve, lato) e la regola delle tolleranze dell'[ADR
  0009](0009-regola-delle-tolleranze-del-piano.md). Nessun componente nuovo e
  nessuna dipendenza nuova.
- **Su che cosa si basa:** il testo della richiesta e l'analisi dei suoi casi
  (tabella sotto); la tecnica nota dell'arrangiamento planare (le curve divise
  nei punti in cui si incontrano, le facce tracciate con l'ordine angolare
  esatto dei pezzi in ogni vertice); il comportamento di `HATCH` di AutoCAD
  come lo descrive la sua documentazione (punto interno, selezione di oggetti,
  stili *normale*, *esterno*, *ignora*).
- **Non verificato:** nessun prototipo è stato eseguito, quindi le prestazioni
  sono un obiettivo da misurare nella tappa. Il comportamento di AutoCAD sulle
  isole che toccano un'altra curva e sui piccoli spazi non è stato provato:
  dove il contratto sceglie diversamente, lo dice.

### Che cosa rende la risposta coerente

Il problema ha tre parti, e solo la prima è nuova come tecnica:

1. **La topologia, esatta.** Con le curve divise in tutti i punti in cui si
   incontrano (decisi esattamente dall'ADR 0010) e i pezzi ordinati in ogni
   vertice per direzione tangente, e a pari tangente per curvatura, le facce
   del piano sono determinate senza tolleranze. La faccia che contiene il
   punto, i suoi bordi, le isole e la loro profondità si leggono da lì. Due
   sistemi che decidono esattamente gli stessi segni danno la stessa
   topologia.
2. **Che cosa intendeva l'utente, con la regola dell'ADR 0009.** Nei disegni
   reali le curve che «si toccano agli estremi» spesso non si toccano
   esattamente: l'estremo di un arco nasce da un angolo e non coincide mai bit
   per bit con l'estremo di una linea; `TRIM` lascia l'estremo tagliato a un
   arrotondamento dalla curva che taglia. Con le sole decisioni esatte una
   regione disegnata chiusa risulterebbe aperta. L'ADR 0009 dice come si
   rimedia: la regola cambia i dati **prima** delle decisioni, mai al loro
   posto. Qui i cambiamenti sono due: un **collegamento**, il segmento fra due
   estremi distinti entro la tolleranza delle loro curve, e un
   **prolungamento**, la continuazione oltre un estremo fino al primo
   incontro entro la tolleranza (come `EXTEND`).
3. **La costruzione dei contorni restituiti**, arrotondata entro un errore
   dichiarato (ADR 0013): vertici e bulge di polilinee chiuse, che Tecnigrafo
   usa così come sono per i contorni del retino nel DXF e per la regione
   piena.

Perché tutte le estremità siano `double`, e i collegamenti siano segmenti fra
`double` che le decisioni dell'ADR 0010 trattano già, gli archi (`Arc`) entrano
come segmenti di polilinea con bulge, fra i loro estremi arrotondati
(`endpoints`), entro metà dell'errore dichiarato. Le decisioni che dipendono da
questo spostamento (un arco che sfiora un'altra curva) cadono in una fascia
(ADR 0013, punto 4). I cerchi e i segmenti di polilinea restano esattamente i
loro dati.

### I casi della richiesta

| Caso | Risposta del contratto |
|---|---|
| 1. Rettangolo di quattro linee che si toccano agli estremi, cerchio dentro; punto fra i due | `enclosed`: contorno il rettangolo (quattro pezzi), un'isola, il cerchio (restituito come `Circle`), profondità 1 |
| 1 bis. Stesso disegno, punto dentro il cerchio | `enclosed`: contorno il cerchio, nessuna isola |
| 2. Isola in un'isola nel contorno | `enclosed`: isole a profondità 1 e 2 |
| 3a. Due linee che sporgono oltre l'angolo | le sporgenze hanno la stessa faccia dai due lati e restano fuori; il contorno passa per il punto d'incrocio |
| 3b. Curve tangenti, tratti sovrapposti | il punto di tangenza è un vertice; un tratto comune è un solo pezzo, attribuito alla curva di indice minore |
| 3c. Polilinea che interseca se stessa | i suoi pezzi dividono il piano come ogni altra curva: il contorno è il bordo della faccia del punto |
| 4. Contorno aperto per un piccolo spazio | spazio entro la tolleranza: chiuso da un collegamento, `enclosed`; più largo: `notEnclosed`, con gli estremi liberi del gruppo di curve aperto in `openEnds` |
| 5. Punto sopra una curva del contorno | `onCurve`, con l'indice della curva |
| 6a. Contorni scelti: rettangolo, due cerchi dentro, una polilinea chiusa in uno dei due | profondità 0, 1, 1, 2 |
| 6b. Due contorni che si toccano in un punto | da fuori: nessuno racchiude l'altro (profondità 0 e 0); da dentro: quello interno ha profondità 1 |
| 6c. Due contorni che si intersecano | conflitto `crossing`, nessuno dei due conta per la profondità dell'altro |
| 7. `autocad-retini.dxf` | casi dell'archivio, scritti dal pacchetto di riferimenti della tappa |

Le risposte esatte, con le coordinate, le scrive il pacchetto di riferimenti
(Verifica).

## Alternative considerate

- **Esito 1 o 2 (OCCT):** esclusi dall'ADR 0030 di Tecnigrafo, punto 5.
- **Esito 4:** il bisogno spetta a solid-space (sopra; ADR 0048 di Tecnigrafo,
  punto 5).
- **Una libreria esistente.** L'arrangiamento di CGAL con le curve circolari
  (`Arrangement_2`, GPL) fa esattamente la parte topologica, ma porta Boost e
  GMP, esclusi da `core` di Tecnigrafo come per l'ADR 0010. Clipper2 lavora su
  poligoni a coordinate intere: gli archi andrebbero spezzati in segmenti, e le
  decisioni non sarebbero più esatte.
- **Approssimare gli archi con poligoni** e usare un algoritmo sui poligoni:
  semplice, ma le tangenze e i tratti sovrapposti diventano scelte
  dell'approssimazione, e il risultato cambia con la sua finezza.
- **Seguire il contorno come AutoCAD** (dal punto, un raggio fino alla prima
  curva, poi lungo le curve girando ogni volta il più possibile): costa meno
  quando le curve sono molte, ma il risultato dipende dalle scelte fatte
  durante il percorso. Il contratto definisce il risultato con l'arrangiamento;
  chi lo realizza può limitare il lavoro alle curve che contano, purché il
  risultato sia lo stesso.
- **Solo decisioni esatte, senza la regola:** le regioni disegnate chiuse con
  archi o con `TRIM` risulterebbero aperte (sopra).
- **Una tolleranza per tutto il disegno, o uno spazio massimo come `HPGAPTOL`
  di AutoCAD:** l'ADR 0009 scarta la tolleranza relativa al disegno intero;
  chiudere spazi più larghi della tolleranza è un'altra funzione, che non è
  stata chiesta.
- **Unire gli estremi spostandoli** invece di collegarli: un estremo d'arco non
  si può portare su un punto dato senza cambiare l'arco, e due estremi da unire
  diventerebbero tre o più a catena (la regola non è transitiva). Il
  collegamento lascia i dati com'erano, si decide a coppie e non dipende
  dall'ordine.
- **Isole come tutte le facce dentro il contorno:** le linee interne di
  un'isola (un cerchio con un diametro) genererebbero isole dentro l'isola, che
  il retino *normale* riempirebbe di nuovo. Il contratto prende di ogni gruppo
  di curve connesse solo il bordo esterno, come un oggetto chiuso.
- **Contorni restituiti solo come riferimenti** (indice della curva e posizioni
  lungo di essa): esatti, ma Tecnigrafo dovrebbe costruire da sé i contorni del
  DXF e della regione piena, cioè fare le costruzioni del kernel. Il contratto
  dà polilinee chiuse e, accanto, l'origine di ogni pezzo.
- **Un solo ADR per #214 e #215:** sono due richieste, con due pezzi del kernel
  che possono andare in parallelo (ADR 0007, punto 4). Condividono i tipi delle
  regioni (`region.hpp`), definiti qui.

## Decisione

1. **Due funzioni in `geom2d`**, in `boundary.hpp`: `detectBoundary`, il
   contorno chiuso più piccolo intorno a un punto, con le isole; e
   `nestContours`, quali contorni scelti stanno dentro quali. I tipi delle
   regioni stanno in `region.hpp`: un `Loop` è un cerchio o una polilinea presa
   come chiusa, e una regione è una lista di loop con la regola pari-dispari.
2. **Il risultato di `detectBoundary` è definito da passi sui valori esatti**,
   scritti nell'intestazione: dati e scarti; collegamenti; prolungamenti;
   punto sopra una curva; arrangiamento e faccia del punto; contorno e isole;
   profondità. Ogni confronto è esatto. La tolleranza dell'ADR 0009 entra solo
   nei passi che cambiano i dati (collegamenti, prolungamenti, punto sopra una
   curva), con la M delle curve in gioco. Due estremi distinti si collegano
   se la loro distanza esatta è al più `policy.epsilon(M)` per le loro
   curve. Nei passi 2, 3 e 4 la M di una curva è quella della curva data
   (`magnitude`, `curves.hpp`): per un `Arc` convertito, quella dell'arco
   dato. Il prolungamento del passo 3 non cambia la M della curva. Nel
   passo 4 M conta anche le coordinate del punto scelto.
   Da un estremo libero senza collegamento si percorre il
   prolungamento verso l'esterno, sulla semiretta oltre l'estremo o sulla
   parte restante del cerchio oltre la fine nel verso del pezzo, oltre
   l'inizio contro il verso.
   Si prende il primo punto comune con un'altra curva del passo 1, comprese
   le tangenze esatte. Se la distanza esatta dall'estremo al punto è al più
   epsilon per la curva prolungata e una curva incontrata in quel punto,
   il pezzo originale si prolunga fino al punto. Altrimenti l'estremo resta
   libero. Quando più curve passano per quel punto, i pezzi seguono il
   passo 5 di `detectBoundary`.
3. **Gli archi entrano come segmenti di polilinea** fra i loro estremi
   arrotondati, divisi in due oltre mezzo giro, entro metà di
   `kRelativeLoopError`; un arco intero diventa il suo cerchio. Le decisioni che
   cambierebbero spostando un arco di quell'errore sono libere (fascia, ADR
   0013, punto 4); fuori dalla fascia sono determinate.
4. **Il contorno** è il bordo della faccia del punto che la racchiude. **Le
   isole** sono gli altri loop della faccia, anche se toccano il contorno,
   e il bordo esterno di ogni altro gruppo di curve connesse dentro il
   contorno. Un pezzo con la stessa faccia su entrambi i lati non fa parte
   di un'isola. **La profondità** di un loop è il numero degli altri loop che
   lo racchiudono. Con lo stile *normale* contano tutti i loop
   (pari-dispari), con *esterno* quelli di profondità 0 e 1, con *ignora* solo
   il contorno: lo applica Tecnigrafo.
5. **Tre esiti**: `enclosed`, con i loop; `onCurve`, quando il punto sta entro
   la tolleranza di una curva, con l'indice; `notEnclosed`, quando il punto non
   è racchiuso, con gli estremi liberi del primo gruppo di curve aperto
   incontrato dalla semiretta dal punto verso +x. Una curva non valida non fa
   fallire la chiamata: si scarta e si elenca, come in `intersectAlong`. Solo
   un punto non valido dà una diagnosi.
6. **I loop restituiti** girano in senso antiorario. Un giro intero è
   restituito come `Circle`, con i suoi `double`, solo se tutti i pezzi del
   giro sono accreditati allo stesso `Circle` o `Arc` intero di ingresso
   dopo il passo 5. Gli altri loop sono polilinee chiuse che cominciano
   dal vertice minore in ordine lessicografico esatto, con un pezzo d'arco al
   più di mezzo giro, gli estremi dei dati copiati, gli altri vertici entro
   `kRelativePointError` e ogni pezzo entro `kRelativeLoopError` (2^-44) del
   tratto esatto. Accanto a ogni pezzo, la sua origine: la curva e il verso, o
   il collegamento con le due curve. L'ordine dei loop e dei pezzi è esatto.
7. **`nestContours` è esatta e senza tolleranza**: un contorno racchiude un
   altro quando qualche punto dell'altro è dentro e nessuno è fuori;
   l'interno di un contorno è pari-dispari, quindi le polilinee che si
   intersecano sono ammesse. Due contorni che si attraversano o coincidono sono
   elencati come conflitto e non contano l'uno per la profondità dell'altro.
8. **Determinismo:** la regola dell'ADR 0008, punto 5. Stessi dati, stessi bit
   sui tre sistemi.
9. **Prestazioni**, obiettivo da misurare nella tappa: qualche migliaio di
   curve candidate, con le intersezioni di un disegno tecnico, in poche decine
   di millisecondi. La realizzazione può evitare di calcolare tutte le coppie
   (ingombri, una griglia, il solo gruppo di curve intorno al punto), purché il
   risultato non cambi. La misura, con il banco di prova, entra nelle note di
   rilascio, come per l'ADR 0010.
10. **Obiettivi raggiungibili adottati.** Si garantiscono le decisioni esatte,
    la regola delle tolleranze come scritta e gli errori dichiarati. **Non** si
    garantiscono, e il risultato può differire da AutoCAD:
    - la chiusura di uno spazio più largo della tolleranza (niente `HPGAPTOL`);
    - il prolungamento di un estremo il cui primo incontro lungo l'estensione
      è oltre la tolleranza, anche se l'estremo è vicinissimo a una curva
      (per esempio una quasi tangenza): l'estremo resta libero;
    - in un gruppo diverso da quello del contorno, un'isola che tocca
      dall'interno un'altra curva del suo gruppo conta solo con il bordo
      esterno del gruppo;
    - gli estremi liberi di `notEnclosed` sono un'indicazione: quelli del primo
      gruppo aperto lungo la semiretta, non tutti gli spazi del disegno.

## Contratto

Intestazioni pubbliche in bozza in questa pull request (ADR 0007, punto 15):

- `geom2d/include/solid_space/geom2d/region.hpp`: `Loop` (`Circle` o
  `Polyline` presa come chiusa), `LoopPieceSource`, `LoopPieceOrigin`,
  `RegionLoop` (loop, origini dei pezzi, profondità), `SkippedInput`, e
  l'errore garantito dei pezzi dei loop, `kRelativeLoopError` = 2^-44, quattro
  volte sotto la tolleranza relativa dell'ADR 0009.
- `geom2d/include/solid_space/geom2d/boundary.hpp`: `BoundaryOutcome`,
  `OpenEnd`, `DetectedBoundary`, `detectBoundary`; `ContourConflictKind`,
  `ContourConflict`, `ContourNesting`, `nestContours`. L'intestazione scrive i
  passi del risultato esatto, gli errori dei vertici (gli estremi dei dati
  copiati, gli altri entro `kRelativePointError`) e dei pezzi, la fascia degli
  archi, l'ordine dei loop e dei pezzi, gli scarti e le diagnosi.

Le diagnosi sono quelle di `diagnosis.hpp`, senza codici nuovi: per
`detectBoundary` `invalidValue` o `outOfDomain` sul punto (input 0); le curve
scartate, con `invalidValue`, `outOfDomain` (anche per un punto della curva, o
del cerchio di un pezzo curvo, oltre `kMaxCoordinate`) o `degenerateCurve`,
stanno in `skipped` con input i + 1. `nestContours` non dà diagnosi: i contorni
non validi stanno in `skipped`. Nessuna intestazione già accettata cambia. Le
funzioni sono solo dichiarate; il test `tests/geom2d/contract_test.cpp`
controlla che le intestazioni compilino con le firme dichiarate.

## Conseguenze

- Componente: `geom2d`, con due intestazioni nuove; nessuna dipendenza nuova.
- **Per Tecnigrafo** (lo decide il responsabile): la facciata dei retini passa
  a `detectBoundary` le curve candidate tradotte in `Curve` (le polilinee con
  `pieces`) e la `TolerancePolicy` del disegno; usa i `RegionLoop` così come
  sono per i contorni del `HATCH` (polilinee con bulge) e per gli anelli della
  `FilledRegion`; sceglie i loop per lo stile delle isole con `depth`; con i
  contorni scelti traduce cerchi e polilinee chiuse in `Loop` e usa
  `nestContours`. Per un retino `SOLID` con lo stile *normale* la regola
  pari-dispari della regione basta già (ADR 0048 di Tecnigrafo, punto 3).
- **Per la richiesta #215** (parti di una curva dentro una regione): la sua
  valutazione descrive la regione con i tipi di `region.hpp`, così il risultato
  di `detectBoundary` e i contorni scelti si passano così come sono.
- **Per la richiesta #217** (ellisse): se la sua valutazione aggiunge l'ellisse
  a `Curve`, decide anche come la tratta `detectBoundary`, con la revisione del
  contratto; oggi i contorni con lati d'ellisse non sono gestiti da Tecnigrafo
  (ADR 0048 di Tecnigrafo, punto 1).
- **Rischi.** L'ordine angolare esatto dei pezzi in un vertice, con tangenze e
  curvature uguali, è la parte delicata della topologia; le prestazioni con
  migliaia di curve chiedono di non calcolare tutte le coppie; le facce con i
  bordi che si toccano in un punto chiedono casi propri. La tappa dedica ai
  primi due un pacchetto a sé, con riferimenti di fascia forte.
- **Pacchetti, in via indicativa** (il piano lo scrive il responsabile
  all'apertura): l'arrangiamento (divisione delle curve, tratti comuni, ordine
  nei vertici, facce); `detectBoundary` sopra l'arrangiamento (regola, faccia
  del punto, contorno, isole, estremi liberi, costruzione dei loop);
  `nestContours`; ognuno con i suoi riferimenti, dopo la revisione del
  contratto `[0.3-CS]`.
- Fuori da questa richiesta: il taglio delle righe del motivo (#215), lo
  spazio massimo `HPGAPTOL`, i contorni con ellissi o spline, l'associatività
  del retino ai contorni.

## Verifica

- **Casi di riferimento** in `tests/corpus/casi/geom2d/`, scritti prima da un
  agente diverso da chi scrive il codice (ADR 0004), con dati in `double` come
  Tecnigrafo li darebbe:
  - i casi della richiesta (tabella sopra), alle scale dell'ADR 0009
    (millimetri vicino all'origine, chilometri con coordinate di 1e5);
  - la regola: due estremi a distanza uguale alla tolleranza e un `double`
    oltre; un estremo tagliato da `TRIM` a un arrotondamento dalla curva; un
    arco raccordato a due linee (`FILLET`); un primo incontro tangente entro
    la tolleranza (si prolunga) e un primo incontro oltre la tolleranza su
    una curva vicina all'estremo (resta libero). I dati di #223–#226
    distinguono la scala dei collegamenti e l'ordine degli incontri;
  - la topologia: vertici con quattro o più pezzi, tangenze interne ed esterne
    nello stesso vertice, tratti sovrapposti da più curve, facce con il bordo
    che tocca se stesso, isole che toccano il contorno, gruppi di curve dentro
    un'isola, curve aperte dentro la regione. I dati di #227 distinguono
    l'isola tangente al contorno e quelli di #228 un giro con origini miste;
  - `notEnclosed` con e senza estremi liberi; il punto a distanza uguale alla
    tolleranza da una curva e un `double` oltre;
  - `nestContours`: annidamenti, contorni che si toccano da fuori e da dentro,
    che si attraversano, uguali, polilinee che si intersecano, contorni non
    validi;
  - i casi di `autocad-retini.dxf`, quando Tecnigrafo lo pubblica (ADR 0007,
    punto 17).
- **Valori attesi** indipendenti dal programma: le decisioni (esito, loop come
  successioni di pezzi con la loro origine, profondità, conflitti, scarti) con
  aritmetica esatta (`fractions` di Python, numeri della forma a + b·√d come
  nell'ADR 0010); le coordinate dei vertici con il modulo `decimal`, confrontate
  con la tolleranza dell'errore garantito (ADR 0013, punto 5). Nessun caso fissa
  un esito dentro la fascia degli archi.
- **Proprietà**, provate dai test del pacchetto su dati generati in modo
  deterministico: permutando le curve, i loop restano gli stessi entro gli
  errori e le origini si permutano; ogni loop di profondità k è racchiuso da
  esattamente k altri loop del risultato; con l'esito `enclosed`, il risultato
  non cambia aggiungendo fuori dal contorno curve che non toccano le altre;
  quando i loop di un `enclosed` non si toccano, `nestContours` sui loop
  restituiti ritrova le loro profondità (se si toccano, l'arrotondamento può
  farli attraversare).
- **Determinismo:** il job della CI che confronta i risultati sui tre sistemi
  (ADR 0008, punto 5) include questi casi.
- **Criteri di chiusura**, dal «quando è soddisfatta» della richiesta:
  - [ ] su questi casi `detectBoundary` dà la stessa regione con le stesse
    isole, bit per bit, sui tre sistemi operativi, e quando la regione non è
    chiusa dà `notEnclosed` invece di una regione sbagliata;
  - [ ] con i contorni scelti, `nestContours` dà le stesse profondità sui tre
    sistemi;
  - [ ] il responsabile verifica dal lato di Tecnigrafo che il comando dei
    retini, nella sua CI, riempie le stesse regioni sui tre sistemi.
