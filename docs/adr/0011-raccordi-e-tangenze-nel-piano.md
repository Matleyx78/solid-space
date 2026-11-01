# ADR 0011: Raccordi e tangenze tra linee, archi e cerchi del piano

- **Stato:** Accettato
- **Data:** 2026-10-29
- **Corretto:** 2026-11-01 — due punti che il contratto non determinava,
  trovati realizzando `tangentCircles` (pacchetto P15, issue #66, pull request
  #120) e confermati dal responsabile: (1) un centro di cerchio tangente oltre
  `kMaxCoordinate` (due rette quasi parallele) dà la diagnosi `outOfDomain` sul
  raggio (dato 2), perché `TangentCircles`, diversamente dalle intersezioni
  dell'ADR 0010, non ha un conteggio dei punti omessi e nessun cerchio si toglie
  in silenzio (punto 8); (2) il cerchio che coincide con una curva circolare
  (stesso centro, raggio uguale) la tocca ovunque e non in un punto di
  tangenza, quindi non è un cerchio tangente e non si elenca (punto 5).
- **Corretto:** 2026-11-01 — un punto che il contratto di `fillet` non
  determinava, trovato realizzando `fillet` (pacchetto P16, issue #70, pull
  request #123) e deciso dal responsabile: quando la regola 3 incontra un centro
  di cerchio tangente oltre `kMaxCoordinate`, o la regola 2 (raggio zero) uno
  spigolo oltre `kMaxCoordinate`, `fillet` dà la diagnosi `outOfDomain` sul
  raggio (dato 2), come `tangentCircles`, e non `none`: il raccordo esiste, ma
  fuori dal dominio (punti 5 e 8).
- **Corretto:** 2026-11-01 — decisioni della revisione C (#133), issue
  #135–#137, #140 e #142: ordine totale dei cerchi (tangenze, poi centro
  esatto x/y), dominio di tutti i dati costruiti con diagnosi dell'intera
  chiamata, compresa la regola 1 di `fillet`; pavimento 2^-1022 degli errori
  e ordine generale delle diagnosi in `curves.hpp` (punti 3, 7 e 8).
  Dopo la revisione delle correzioni, #149–#152 precisano dominio sul valore
  esatto e nessun valore restituito oltre K, dati prima delle costruzioni e
  `ambiguousPick` prima dei candidati, soli punti elencati di `tangentsFrom`
  controllati, bulge zero se le tangenze restituite coincidono (punti 4, 6 e 8).
- **Richiesta:** #21 — da Tecnigrafo: tappa 0.7, Matleyx78/tecnigrafo#327
- **Esito:** 3 pezzo nostro
- **Tappa:** 0.2 (scheda da scrivere all'apertura)

## Richiesta

Tecnigrafo chiede tre costruzioni di tangenza tra segmenti, archi e cerchi del
piano, anche quando sono pezzi di una polilinea: il raccordo di raggio dato tra
due curve, dal lato che l'utente indica con un punto su ciascuna (per due
segmenti paralleli, il semicerchio come in AutoCAD); il cerchio di raggio dato
tangente a due curve, vicino ai due punti indicati; i punti di tangenza da un
punto a un cerchio o a un arco, con la decisione se il punto sta fuori, sul
cerchio o dentro. Servono a `FILLET`, allo snap a tangente e a `CIRCLE`
tangente-tangente-raggio. Il testo completo è nella issue #21.

## Valutazione

- **Spetta a solid-space.** Tangenze e raccordi del piano sono un compito di
  `geom2d` ([ADR 0008](0008-componente-geom2d.md), punto 2, prerequisito di
  questa valutazione), e la scelta del lato deve venire dalle stesse decisioni
  esatte della richiesta #20 ([ADR 0010](0010-posizione-e-intersezioni-nel-piano.md)).
- **Esiti 1 e 2 esclusi da un vincolo già preso:** `core` di Tecnigrafo compila
  senza librerie esterne, quindi senza OCCT (ADR 0030 di Tecnigrafo, punto 5;
  [ADR 0007](0007-tappe-su-richiesta.md), punto 8).
- **Esito 3**, in `geom2d`, sopra le curve e le decisioni dell'ADR 0010 e con
  la regola delle tolleranze dell'[ADR 0009](0009-regola-delle-tolleranze-del-piano.md).
- **Su che cosa si basa:** il testo della richiesta, il calcolo a mano dei suoi
  casi (sotto) e, come informazione, la descrizione dei casi di
  `autocad-raccordi.dxf` e la bozza dell'ADR 0035 di Tecnigrafo nella
  Matleyx78/tecnigrafo#330 (non ancora unita). **Non verificato:** nessun
  prototipo è stato eseguito, quindi le prestazioni sono un obiettivo da
  misurare; e non si è potuto confrontare con AutoCAD la regola dell'arco più
  corto (Decisione, punto 6) nelle configurazioni in cui il cerchio di
  raccordo racchiude l'altro cerchio, che i file di prova non contengono.

### Non è il motore di raccordi dell'appendice «Non ora»

L'appendice "Non ora" di [VISIONE.md](../VISIONE.md) rimanda il **motore di
raccordi proprio**: i raccordi sugli spigoli dei solidi del `kernel` (rolling
ball, cliff edge, cuspidi, raccordi di vertice con patch di Gregory), cioè la
sostituzione del modulo dei raccordi di OCCT, che si propone solo con i dati
dell'archivio e una soglia dichiarata prima (ADR 0007, punto 6). Questa
richiesta è un'altra cosa: un arco di cerchio tangente a due curve del piano.
È una costruzione elementare in forma chiusa (il centro è un punto comune di due
curve parallele), sta in `geom2d`, non tocca superfici né solidi e non
sostituisce nessun modulo di OCCT. Quell'idea resta nell'appendice, e i
raccordi dei solidi, quando una richiesta li porterà, partiranno da OCCT dietro
la facciata `kernel`.

### Che cosa rende possibili decisioni esatte

Il centro di un cerchio di raggio r tangente a due curve è un punto comune
delle loro curve parallele a distanza r: rette spostate di r lungo la normale,
cerchi di raggio R + r o |R − r|. La normale di un segmento porta una radice
(la sua lunghezza), il cerchio di un segmento di polilinea curvo ha centro e
raggio al quadrato razionali (ADR 0010). Le coordinate dei centri e dei punti
di tangenza sono quindi numeri algebrici di grado limitato nei `double`
ricevuti: esistenza, numero, lato, posizione lungo ciascuna curva e verso
dell'arco si decidono esattamente con un numero fisso di elevamenti al
quadrato, con un filtro in virgola mobile nel caso comune, come nell'ADR 0010.

### I casi della richiesta

| Caso | Risposta esatta |
|---|---|
| 1. Segmenti perpendicolari che si incontrano in un angolo, raggio 10 | un raccordo: centro a 10 da ciascun segmento, nel quarto indicato dai punti; punti di tangenza a 10 dall'angolo, all'interno dei segmenti se sono lunghi più di 10; arco di un quarto di giro |
| 2a. Segmento e cerchio che si intersecano | dal lato indicato, i cerchi di raccordo esterni al cerchio stanno sulla parallela al segmento e a R + r dal centro: uno vicino a ciascuna intersezione; si sceglie il più vicino ai punti |
| 2b. Segmento e cerchio che non si toccano | dal lato in cui la retta e il cerchio si guardano, il raccordo esiste se e solo se la loro distanza è al più 2r, deciso esattamente; uguale a 2r, un solo cerchio |
| 3. Due archi concentrici | nessun raccordo per ogni raggio, tranne quando i raggi differiscono, o si sommano, di esattamente 2r: allora i cerchi sono infiniti e la risposta lo dice (`infinitelyMany`). Non si sceglie a caso |
| 4. Raggio 1e-9 su due segmenti quasi allineati | il raccordo esiste (le rette non sono parallele) e i punti di tangenza stanno a r · tan(φ/2) dall'angolo, con φ l'angolo di cui gira la direzione; con coordinate dell'ordine di 10 l'errore (circa 3.6e-14) è molto sotto r |
| 5. Segmenti paralleli a distanza 20 | il semicerchio di raggio 10, qualunque sia il raggio chiesto, dall'estremo del primo segmento più vicino al punto indicato |
| 6. Tangenti da un punto sul cerchio; da un punto a raggio + 1e-12 dal centro | sul cerchio: `on`, il punto stesso, senza arrotondamento; a R + 1e-12 (centro nell'origine, R = 5): `outside`, due punti a y ≈ ±3.16e-6 |
| 7. Gli stessi casi a coordinate dell'ordine di 1e6 | la risposta esatta per i dati traslati, che sono già arrotondati: 1e6 + 5 + 1e-12 vale 1e6 + 5 in `double`, quindi nel caso 6 il punto sta `on`. Nel caso 4 l'errore dei punti (circa 3.6e-9) supera r = 1e-9: il raccordo esiste, ma è sotto la tolleranza dell'ADR 0009 (circa 2.3e-7), e Tecnigrafo lo riconosce con `isDegenerate` |

## Alternative considerate

- **Esito 1 o 2 (OCCT, `Geom2dGcc_Circ2d2TanRad`):** esclusi dall'ADR 0030 di
  Tecnigrafo, punto 5.
- **Esito 4, le costruzioni in Tecnigrafo:** Tecnigrafo dovrebbe rifare sopra i
  predicati le decisioni di lato e di posizione, e due implementazioni delle
  stesse decisioni possono contraddirsi. Restano a Tecnigrafo il comando
  (che cosa si taglia, *Trim*, *Polyline*, *Multiple*) e lo smusso, che usa
  solo le intersezioni dell'ADR 0010.
- **Una libreria esistente** (le costruzioni di Apollonio del kernel circolare
  di CGAL): porta Boost e GMP, e `core` non ha dipendenze esterne (ADR 0030 di
  Tecnigrafo, punti 4 e 5).
- **Solo `tangentCircles`, con la scelta lasciata a Tecnigrafo:** la scelta del
  lato userebbe stazioni e punti arrotondati invece delle decisioni esatte, e
  la richiesta chiede che sia coerente con la #20. Il contratto dà entrambe: i
  candidati e la scelta.
- **Anche la scelta tra candidati dallo stesso lato esatta** (il più vicino ai
  punti indicati): significa confrontare somme di distanze tra numeri
  algebrici diversi, un calcolo molto più pesante, per una domanda che non ha
  una risposta "vera": quando due candidati sono quasi alla stessa distanza dai
  clic, è l'utente a non aver indicato quale vuole. La scelta resta
  deterministica, e il lato, che è la parte geometrica, resta esatto.
- **Raggio zero come diagnosi:** AutoCAD usa il raggio 0 per lo spigolo, e il
  semicerchio tra parallele non dipende dal raggio; un raggio zero valido evita
  a Tecnigrafo un ramo a parte per questi due casi.
- **L'arco solo come `Arc`:** i raccordi tra pezzi di polilinea diventano un
  segmento con bulge; ricavare il bulge dagli angoli arrotondati aggiungerebbe
  errore. Il contratto dà le due forme.
- **Una diagnosi per "non esiste":** non è un errore nei dati ma una risposta.
  È un valore (`TangencyOutcome`), e i codici di diagnosi dell'ADR 0010 non
  cambiano.

## Decisione

1. **Le costruzioni stanno in `geom2d`**, nell'intestazione `tangency.hpp`,
   sopra le curve e le decisioni dell'ADR 0010.
2. **Quattro funzioni:** `tangentsFrom` (tangenti da un punto, per lo snap a
   tangente), `tangentCircles` (tutti i cerchi di un raggio tangenti a due
   curve, al più otto), `tangentCircleNear` (`CIRCLE` tangente-tangente-raggio)
   e `fillet` (`FILLET`). Ogni curva si prende con il suo prolungamento, la
   retta o il cerchio intero, così le curve non devono toccarsi; la posizione
   dei punti di tangenza (`CurvePosition` dell'ADR 0010) dice a Tecnigrafo come
   tagliare o prolungare ciascuna curva.
3. **Ogni decisione è esatta:** dove sta un punto rispetto a un cerchio
   (`Placement`), se esistono cerchi tangenti e quanti, da che parte di ogni
   curva sta un cerchio tangente (`Side`, con lo stesso significato di
   `side()`), dove stanno i punti di tangenza su ogni curva, il loro ordine, se
   due rette sono parallele, il verso dell'arco di raccordo, e quando i cerchi
   sono infiniti. I cerchi si ordinano per tangenza esatta sulla prima curva,
   poi sulla seconda, poi per centro esatto, prima x e poi y: ordine totale
   per il raggio dato (#135), ereditato dai pareggi di `tangentCircleNear`
   e `fillet`.
4. **La scelta del lato è quella della #20.** Il raccordo sta, rispetto alla
   prima curva, dalla parte in cui `side()` mette il punto indicato sulla
   seconda, e viceversa: ogni punto indica la parte della sua curva da tenere.
   Se un punto sta esattamente sulla retta o sul cerchio dell'altra curva la
   risposta è `ambiguousPick`, prima delle costruzioni e della loro diagnosi
   di dominio; poi `infinitelyMany`, poi i candidati (#150). Tra i candidati rimasti, e per `CIRCLE`
   tangente-tangente-raggio, si sceglie quello con i punti di tangenza più
   vicini ai punti indicati: la scelta è deterministica, calcolata sui punti
   arrotondati con un'espressione fissata, ma non esatta (vedi le
   alternative).
5. **Casi particolari, come AutoCAD:** due curve dritte su rette parallele
   distinte danno il semicerchio, qualunque sia il raggio; il raggio zero dà
   lo spigolo, cioè il punto comune delle due curve prolungate; due curve sulla
   stessa retta o sullo stesso cerchio, e i casi con infiniti cerchi tangenti,
   danno `infinitelyMany`; un raccordo che non entra dà `none`. "Non esiste"
   arriva solo quando la costruzione esattamente non esiste. Il cerchio che
   coincide con una curva circolare (stesso centro, raggio uguale) non è un
   cerchio tangente: la tocca ovunque, non in un punto (corretto il
   2026-11-01).
6. **L'arco di raccordo** è il più corto dei due archi del cerchio di raccordo
   tra i punti di tangenza, con il verso deciso esattamente; si dà nelle due
   forme che Tecnigrafo usa: un `Arc` (raggio dato esattamente, angoli da
   `robust::atan2`) e il bulge di un segmento di polilinea. Se le tangenze
   restituite coincidono, il bulge è 0; l'`Arc` mantiene il raggio dato e le
   proprie regole (#152). Non si impone il double più vicino alle tangenze:
   la coincidenza può dipendere da una scelta lecita entro l'errore. Un caso
   al confine non può imporre né coincidenza né una delle due forme del bulge;
   si controlla condizionatamente ai punti restituiti. La forma con bulge può
   essere un punto: in quel caso Tecnigrafo usa l'`Arc`. Raggio zero e punti
   esattamente coincidenti continuano a dare i risultati puntiformi previsti.
7. **Errori dichiarati.** Centri e punti di tangenza entro
   `kRelativePointError` (2^-48) · max(M, |p|, 2^-1022), stazioni entro
   `kRelativeStationError` (2^-46) · max(M, |s|, 2^-1022), come nella regola
   generale di `curves.hpp`. Il pavimento (#139, #140) vale per tutte le
   costruzioni geom2d, senza cambiare decisioni esatte, dominio o diagnosi. L'arco, nelle due
   forme, entro `kRelativeArcError` = 2^-46 · max(M, |p|, 2^-1022) dall'arco esatto, e
   i suoi estremi dai punti di tangenza esatti: gli angoli passano da punti
   già arrotondati, quindi l'errore è più largo di quello dei punti, ma resta
   16 volte sotto la tolleranza relativa dell'ADR 0009, così gli estremi
   dell'arco coincidono, per quella regola, con i punti di tangenza.
8. **Dominio e diagnosi** come nell'ADR 0010: coordinate e raggi fino a
   `kMaxCoordinate`; dati non validi o fuori dominio danno una diagnosi; un
   raggio negativo o non finito è `invalidValue`. I codici di diagnosi non
   cambiano. Un centro di cerchio tangente oltre `kMaxCoordinate` dà
   `outOfDomain` sul raggio (dato 2) e nessun cerchio: i cerchi tangenti non
   hanno un conteggio dei punti omessi come le intersezioni (corretto il
   2026-11-01). Lo stesso vale per `fillet`, anche per lo spigolo del raggio
   zero oltre `kMaxCoordinate` (corretto il 2026-11-01). La revisione C estende
   il controllo a tutti i dati costruiti, mai all'estensione geometrica:
   coordinate, raggi e punto m della forma con bulge devono stare nel dominio
   di `curves.hpp`, senza un divieto generale di risultati degeneri. Il
   dominio si decide sui valori esatti; un esatto entro K non si restituisce
   oltre K (si limita l'arrotondamento a ±K). Il punto m si ricostruisce
   esattamente dai `double` restituiti (#149). Un punto di tangenza oltre il dominio fa fallire
   l'intera chiamata: `outOfDomain`, dato 1 per `tangentsFrom`, dato 2 per
   `tangentCircles`, `tangentCircleNear` e `fillet`; nessuna omissione.
   In `tangentsFrom` si controllano solo i punti elencati: `Extension::none`
   esclude dal controllo quelli fuori dall'arco dato, anche oltre K; con
   `Extension::extended` si controllano tutti (#151).
   Per `fillet` vale anche nella regola 1 (centro, raggio costruito, tangenze
   e piede della perpendicolare). L'ordine delle diagnosi è quello generale
   di `curves.hpp`: dato 0 per intero, poi 1, 2, 3, 4; `notCircular` segue i
   controlli della curva 0 e precede il punto 1. In `fillet` il raggio viene
   controllato anche per rette parallele, prima delle regole geometriche.
   Prima si controllano tutti gli ingressi, anche i punti 3 e 4, poi le
   costruzioni e il loro dominio, anche se la diagnosi porta il dato 2 (#150).
9. **Determinismo:** la regola dell'ADR 0008, punto 5, anche per la scelta del
   punto 4, che è calcolata in virgola mobile.
10. **Prestazioni**, obiettivo da misurare nella tappa: `tangentsFrom`, che lo
    snap chiede a ogni movimento del mouse, costa qualche operazione in virgola
    mobile e una radice nel caso comune; il percorso esatto parte solo vicino
    alle degenerazioni.
11. **Obiettivi raggiungibili adottati.** Si garantiscono le decisioni esatte e
    gli errori dichiarati. **Non** si garantiscono: una scelta esatta tra due
    candidati dallo stesso lato quasi equidistanti dai clic (punto 4); un arco
    di raccordo significativo quando r è sotto l'errore dei punti (caso 7);
    l'arco più lungo dove AutoCAD lo disegnasse. Restano fuori, e a Tecnigrafo:
    lo smusso (che usa le intersezioni dell'ADR 0010), il raccordo di tutti i
    vertici di una polilinea (Tecnigrafo lo compone con `fillet` sui pezzi
    consecutivi dati da `pieces`) e il comportamento dei comandi.

## Contratto

Intestazione pubblica in bozza in questa pull request ([ADR
0007](0007-tappe-su-richiesta.md), punto 15):

- `geom2d/include/solid_space/geom2d/tangency.hpp`: `kRelativeArcError`,
  `Placement`, `TangencyPoint`, `TangentsFromPoint`, `tangentsFrom`,
  `TangentCircle`, `TangentCircles`, `tangentCircles`, `TangencyOutcome`,
  `ChosenCircle`, `tangentCircleNear`, `Fillet`, `fillet`.

Usa senza cambiarli i tipi del contratto dell'ADR 0010 (`curves.hpp`,
`intersection.hpp`, `side.hpp`, `diagnosis.hpp`). Ogni funzione dice nella sua
documentazione a quale dato si riferisce `Diagnosis::input` (2 per il raggio,
3 e 4 per i punti indicati), senza cambiare il tipo. Le funzioni sono solo
dichiarate; il test `tests/geom2d/contract_test.cpp` controlla che
l'intestazione compili con le firme dichiarate, senza chiamare nulla di non
realizzato.

## Conseguenze

- Componente: `geom2d`. Nessuna nuova dipendenza.
- **Per Tecnigrafo** (lo decide il responsabile): la scheda del pacchetto dei
  raccordi (P19 nella bozza della #330) si scrive su `tangency.hpp`; il lato si
  passa come punti indicati, non come `Side`; i punti di tangenza e le loro
  posizioni dicono come tagliare o prolungare; gli esiti `none`,
  `infinitelyMany` e `ambiguousPick` diventano i messaggi del comando; un arco
  sotto la tolleranza si riconosce con `isDegenerate` (ADR 0009). La verità
  geometrica dei casi sta in `tests/corpus/` (ADR 0007, punto 17).
- Il commento di `Diagnosis::input` in `diagnosis.hpp` elenca la numerazione di
  `intersection.hpp`; le funzioni nuove dichiarano la loro nella propria
  intestazione. Se il responsabile preferisce un commento generale in
  `diagnosis.hpp`, è una modifica del contratto (etichetta `contratto`).
- Rischio: come nell'ADR 0010, le decisioni esatte con radici annidate (retta
  spostata contro cerchio spostato) sono la parte delicata. La tappa le dà un
  pacchetto con riferimenti propri, dopo quello delle intersezioni.

## Verifica

- **Casi di riferimento** in `tests/corpus/casi/geom2d/`, scritti prima da un
  agente diverso da chi scrive il codice (ADR 0004):
  - i sette casi della richiesta, con le risposte della tabella sopra, alle due
    scale, con i dati in `double` come Tecnigrafo li darebbe;
  - i casi `FILLET` a–f e `CIRCLE` *Ttr* m e n di `autocad-raccordi.dxf`, che
    Tecnigrafo porta nell'archivio con la sua provenienza (ADR 0007, punto 17);
  - per ogni coppia di tipi (segmento, arco, cerchio, segmento di polilinea) i
    casi limite: raccordo che entra esattamente (un solo cerchio) e di un ulp
    in più o in meno; curve tangenti tra loro; stessa retta, stesso cerchio,
    archi concentrici; punto indicato esattamente sulla curva dell'altro;
    punti di tangenza esattamente agli estremi (`atStart`, `atEnd`); punto
    esattamente sul cerchio e a un ulp da esso; raggio zero; parallele.
- **Valori attesi** da aritmetica esatta: le decisioni con `fractions` di Python
  e confronti tra numeri algebrici (elevamenti al quadrato); i centri, i punti
  e gli archi con il modulo `decimal` ad alta precisione, con la tolleranza del
  caso pari all'errore dichiarato.
- **Proprietà**, provate dai test del pacchetto su dati generati in modo
  deterministico, anche quasi degeneri: ogni cerchio di `tangentCircles` dista
  r da ciascuna curva entro l'errore dichiarato; `tangentCircles(b, a)` dà gli
  stessi cerchi di `tangentCircles(a, b)`, con i lati scambiati; il lato di un
  cerchio tangente rispetto a una curva dritta è `side()` del suo centro;
  `tangentsFrom` dà `on` esattamente quando `side()` dà `on` per lo stesso
  punto.
- **Determinismo:** il job della CI dell'ADR 0008, punto 5, confronta anche
  questi risultati sui tre sistemi operativi.
- **Criteri di chiusura**, dal "quando è soddisfatta" della richiesta:
  - [ ] l'arco e il cerchio sono tangenti alle due curve entro l'errore
    dichiarato, e i punti di tangenza stanno sulle curve entro lo stesso
    errore;
  - [ ] la scelta del lato usa `side()` dell'ADR 0010;
  - [ ] "non esiste" (`none`) arriva solo quando la costruzione esattamente non
    esiste;
  - [ ] i casi della richiesta danno la risposta giusta, con gli stessi bit sui
    tre sistemi operativi; il responsabile lo verifica anche dal lato di
    Tecnigrafo.
