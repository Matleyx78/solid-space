# ADR 0013: Esatto dove serve

- **Stato:** Accettato
- **Data:** 2026-11-02
- **Corregge:** ADR 0010, 0011 e 0012, che rimandano qui con una riga
  **Corretto:**; i loro punti sulle costruzioni si leggono con questo ADR
- **Nasce da:** operazione snellimento, issue #160

## Contesto

Gli ADR 0010–0012 separano le **decisioni**, esatte, dalle **costruzioni**,
arrotondate entro un errore dichiarato. Durante la tappa 0.2 il contratto è
andato oltre questa separazione. Ogni buco trovato si è chiuso fissando un
dettaglio in più dell'arrotondamento:

- i vertici della curva parallela al `double` più vicino, con pareggio al pari
  (#132, #138), perché due scelte lecite degli estremi davano esiti diversi;
- il pavimento 2^-1022 di tutti i limiti d'errore (#139, #140), che ha portato
  i subnormali nel contratto e due pacchetti, R20 e P20;
- tre fasce al confine, scritte una per una (#152, #155, #157).

L'inventario della #160 conta 84 casi del corpus confrontati al bit, 74 nei
file JSON e 10 nel controllo di simmetria di `tangentCircles`, e 15 frasi dei
contratti che prescrivono un arrotondamento, un pavimento o un valore al
confine. Ogni frase di questo tipo costa un pacchetto di lavoro, uno di
riferimenti e una revisione, e lega Tecnigrafo a bit che l'utente non vede: gli
errori delle costruzioni stanno 16 volte o più sotto la tolleranza relativa
dell'ADR 0009.

Quello che l'utente vede sono le decisioni: un lato, un'intersezione che c'è o
non c'è, quali pezzi restano, il messaggio di una diagnosi. Quelle devono
restare esatte. Resta anche il determinismo dell'ADR 0008 (punto 5): senza gli
stessi bit sui tre sistemi non ci sono né il job della CI né il confronto dei
risultati tra versioni.

## Decisione

1. **Esatto dove serve.** Restano esatte, come oggi, le **decisioni**: il lato,
   se e quante volte due curve si incontrano, la coincidenza, quale diagnosi e
   in che ordine, quali pezzi di una curva parallela sopravvivono ai passi del
   suo contratto. Le **costruzioni** (punti, raggi, centri, stazioni, lunghezze,
   bulge) si garantiscono solo entro gli errori massimi già dichiarati:
   `kRelativePointError`, `kRelativeStationError`, `kRelativeArcError`,
   `kRelativeOffsetError`. Il contratto non dice più quale `double` esce: niente
   «il `double` più vicino», niente «pareggio al pari».
   - Restano esatti i valori che il contratto **copia o fissa per struttura**:
     il centro conservato di un arco o di un cerchio spostato, il raggio
     richiesto, l'ingresso restituito a distanza zero, il punto dato di
     `tangentsFrom`, il bulge 0 dei pezzi dritti, gli zeri di un raccordo di
     raggio zero.
   - Restano il **dominio** e le **costanti**: un valore esatto entro
     `kMaxCoordinate` non si restituisce oltre (se l'arrotondamento lo
     supererebbe, si restituisce il confine), e le costanti angolari di
     `curves.hpp` sono i `double` più vicini ai valori esatti. Non sono
     arrotondamenti delle costruzioni.
   - Resta il **determinismo**: stesso ingresso, stessi bit su tutti i sistemi
     (ADR 0008, punto 5). Che due ingressi diversi diano gli stessi bit (per
     esempio scambiando gli argomenti) vale solo dove un'intestazione lo
     promette.
2. **Dominio in basso: 2^-50**, simmetrico a `kMaxCoordinate` = 2^50. Nei
   limiti d'errore delle costruzioni sostituisce il pavimento 2^-1022: per i
   punti l'errore è al più `kRelativePointError` · max(M, |p|, 2^-50), e così
   per stazioni, lunghezze, archi e curve parallele con i loro errori. Sotto
   2^-50 quel limite diventa assoluto e non dice niente della precisione
   relativa: per dati e costruzioni sotto 2^-50, subnormali compresi, il
   contratto garantisce solo niente crash, nessuna eccezione oltre a
   `std::bad_alloc` (ADR 0003, punto 5), niente risultati fuori dominio e il
   determinismo. I subnormali escono dal contratto delle costruzioni. Le
   **decisioni** di `robust` e di `geom2d` restano esatte per ogni `double`
   finito, subnormali compresi: 2^-50 vale solo per i limiti d'errore.
3. **Regola di default dei buchi.** Un buco del contratto è **visibile** quando,
   per dati dentro il dominio, cambia almeno una di queste cose:
   - l'esito o la diagnosi (il messaggio che l'utente legge);
   - il numero o l'ordine delle entità risultanti;
   - una coordinata di più della tolleranza della regola: epsilon(M) dell'ADR
     0009, calcolato da `robust::TolerancePolicy`.

   Un buco visibile lo decide il responsabile, come dice
   [CONTRATTI.md](../CONTRATTI.md). Gli altri li sceglie chi implementa: la
   scelta più semplice, scritta nel commento dell'intestazione, fissata da un
   test e segnalata nella pull request, che allora porta la riga
   **Revisione del contratto: da fare**. Questo ADR autorizza quella modifica
   del commento anche quando la scheda del pacchetto non elenca l'intestazione.
4. **La fascia.** Quando un esito dipende solo da quale `double` una
   costruzione sceglie entro il suo errore dichiarato, quell'esito è **libero
   dentro la fascia**: l'insieme dei dati per cui due scelte lecite danno esiti
   diversi. Per «esito» si intende ciò che il punto 3 chiama visibile (esito o
   diagnosi, numero o ordine delle entità) e la coincidenza di due valori
   restituiti, da cui spesso quelli dipendono. Dentro la fascia l'esito è deterministico, come tutto, ma il
   contratto non lo fissa e nessun caso del corpus lo impone; fuori dalla fascia
   il contratto lo determina. Ogni intestazione che ha esiti di questo tipo
   scrive la sua fascia nel commento della funzione.
   - **Curva parallela** (`offset.hpp`). Due estremi arrotondati possono
     coincidere solo se gli estremi esatti hanno le coordinate che differiscono
     ciascuna al più di 2 · `kRelativePointError` · max(M, |p|, 2^-50), con |p|
     il maggiore dei due. In quella fascia sono liberi lo scarto di un pezzo
     dritto collassato dall'arrotondamento (con `simple`, `origins` e
     `notRepresentable` quando non resta nessun pezzo) e il `failed` di un arco
     con gli estremi arrotondati coincidenti. Il `failed` di un bulge non
     rappresentabile è libero per gli archi quasi interi con la corda di pochi
     ulp, la condizione che `CurveOffset` già scrive. I passi esatti della curva
     parallela, compresi i pezzi che spariscono o si tolgono, non cambiano.
   - **Raccordi** (`tangency.hpp`). Il bulge 0 quando i punti di tangenza
     restituiti coincidono (#152) e il punto m ricostruito al confine di
     `kMaxCoordinate` (#155, con la fascia della #157) sono applicazioni di
     questo principio, come la scelta di `tangentCircleNear` sui punti
     arrotondati.
5. **Il corpus.** I valori costruiti si confrontano con una tolleranza
   dichiarata, ricavata dall'errore massimo del contratto: nessun caso fissa
   quale `double` esce. Restano esatti i valori del punto 1 che il contratto
   copia o fissa per struttura. Un caso con dati dentro una fascia (punto 4) non
   scrive l'esito libero; se serve provarlo, lo prova un test di unità con le
   proprietà che valgono per ogni scelta lecita.

## Alternative considerate

- **Fissare l'arrotondamento dappertutto** («il `double` più vicino»,
  pareggio al pari). Toglie ogni fascia, ma ogni costruzione diventa un calcolo
  arrotondato correttamente, con l'aritmetica esatta fino all'ultimo bit: è il
  peso di P19, e lo stesso servirebbe per tangenze e raccordi. In cambio dà
  bit che Tecnigrafo non usa.
- **Rinunciare anche al determinismo.** Scartata: il job della CI e il
  confronto dei risultati tra versioni ne hanno bisogno, e costa poco (ADR
  0008, punto 5).
- **Tenere il pavimento 2^-1022.** Chiede costruzioni precise nei subnormali
  (R20 e P20) per dati che nessun disegno ha: 2^-50 in metri è meno di un
  millesimo di femtometro. Il dominio simmetrico è più semplice da spiegare e da
  provare.
- **Far decidere ogni buco al responsabile.** È la regola di oggi: nella tappa
  0.2 ha prodotto 27 issue `contratto`, 22 il solo 2026-11-01, molte su
  differenze di un ulp che nessuno vede.

## Conseguenze

- I commenti di `curves.hpp`, `offset.hpp` e `tangency.hpp` seguono questo ADR,
  senza cambiare dichiarazioni. `intersection.hpp` non cambia in questa
  operazione: la sua promessa degli stessi bit scambiando gli argomenti resta.
- Gli ADR 0010, 0011 e 0012 hanno una riga **Corretto:** che rimanda qui; dove
  il loro testo prescrive il `double` più vicino o il pavimento 2^-1022 vale
  questo ADR.
- [AGENTS.md](../../AGENTS.md) e [CONTRATTI.md](../CONTRATTI.md) hanno il
  paragrafo della regola 3; il punto 10 della lista di controllo dichiara la
  fascia invece di fissare il `double`.
- Il corpus: i 22 casi con valori costruiti senza tolleranza passano a una
  tolleranza dichiarata; il controllo di simmetria di `tangentCircles`
  confronta i campi decisi esattamente e le coordinate entro i loro errori.
- Il piano 0.2: P19 perde l'obbligo del `double` più vicino e tiene il resto,
  R19 di conseguenza; P20 diventa «dominio delle tangenze» (#149–#151) e R20
  tiene solo i casi al confine `kMaxCoordinate`, senza subnormali.
- Più facile: meno frasi nel contratto, meno casi fragili, meno issue. Più
  difficile: un caso del corpus deve sapere se i suoi dati cadono in una fascia.
