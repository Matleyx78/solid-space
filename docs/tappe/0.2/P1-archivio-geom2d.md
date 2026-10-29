# [0.2-P1] Archivio: `geom2d`, nomi e dati del piano

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** #…
- **Difficoltà:** facile
- **Agente:** codex
- **Modello e impegno:** leggero; impegno basso
- **Riferimenti:** non servono: il pacchetto prepara il formato in cui si
  scrivono i riferimenti della tappa; lo provano i test del validatore
- **Dipende da:** —
- **Sblocca in Tecnigrafo:** niente direttamente; da qui partono tutti i
  pacchetti di riferimenti della tappa
- **Componente:** nessuno (`tests/corpus/`)
- **Da leggere:** [ADR 0005](../../adr/0005-formato-dei-casi-di-verifica.md)
  (Decisione e Conseguenze), [ADR 0008](../../adr/0008-componente-geom2d.md)
  punto 8, [tests/corpus/README.md](../../../tests/corpus/README.md),
  `tests/corpus/valida_corpus.py` e `test_valida_corpus.py`

## File

- `tests/corpus/valida_corpus.py`
- `tests/corpus/test_valida_corpus.py`
- `tests/corpus/prove/caso-nome-non-valido.json` (nuovo)
- `tests/corpus/README.md`

## Cosa fare

Tre aggiunte al formato, che non cambiano nessun caso esistente.

1. **Il componente `geom2d`** entra nell'elenco dei componenti, dopo `robust`
   (validatore e README, "Disposizione"), come chiede l'ADR 0008, punto 8.
2. **I nomi in `atteso`.** Una stringa di `atteso` che comincia con una lettera
   minuscola e contiene solo lettere e cifre ASCII (`[a-z][A-Za-z0-9]*`) è un
   **nome**: il nome di un enumeratore del contratto (`crossing`, `atStart`,
   `left`, `done`). Una stringa che non è né una frazione né un nome resta un
   errore. È il campo che il README prevede in "Testo che non sia una frazione
   non è ancora previsto"; quella frase cambia di conseguenza. Le due forme non
   si confondono: una frazione comincia con una cifra o con `-`.
3. **Il vocabolario dei dati del piano**, in una sezione nuova del README, "Dati
   del piano (`robust` e `geom2d`)", con il testo qui sotto, adattato alla
   forma del README. Il validatore non lo controlla (la struttura di `ingresso`
   resta libera): serve a chi scrive i casi e a chi li esegue (P2).

### Testo della sezione "Dati del piano"

In `ingresso`:

| Dato | Forma |
|---|---|
| operazione | `"operazione"`: il nome C++ della funzione del contratto (`orient2d`, `intersect`, `offsetPolyline`) |
| numero | un numero JSON, cioè il `double` più vicino a ciò che è scritto, oppure una stringa con il `double` in esadecimale C (`"0x1.0000000000001p+0"`, `"-0x1p-1074"`) quando serve un valore preciso al bit |
| punto | `[x, y]`, due numeri |
| curva | un oggetto con una sola chiave: `{"segmento": {"inizio": punto, "fine": punto}}`; `{"arco": {"centro": punto, "raggio": n, "angolo_iniziale": n, "ampiezza": n}}`; `{"cerchio": {"centro": punto, "raggio": n}}`; `{"pezzo": {"inizio": punto, "fine": punto, "bulge": n}}` (un segmento di polilinea) |
| polilinea | `{"vertici": [[x, y, bulge], …], "chiusa": true}`; il bulge è quello del segmento che parte dal vertice |
| prolungamento | `"none"` oppure `"extended"` |
| unità del disegno | `"unita_in_metri"`: numero (0.001 per i millimetri) |

In `atteso`, oltre alle forme di sempre:

| Dato | Forma |
|---|---|
| nome | il nome dell'enumeratore, come nel C++ (`"atStart"`) |
| punto | `[x, y]`, ogni coordinata esatta o approssimata |
| posizione su una curva | `{"posizione": nome di Location, "stazione": approssimato}` |
| diagnosi | `{"diagnosi": {"codice": nome di DiagnosisCode, "dato": intero esatto}}`; quando la funzione dà una diagnosi, `atteso` ha solo questa chiave |
| conteggi e indici | interi esatti (`"0"`, `"3"`) |

Regole per chi scrive i casi:

- **Le chiavi e le operazioni** di ogni funzione sono nella scheda del pacchetto
  che la realizza (`docs/tappe/0.2/`), sezione "Casi di verifica"; dopo il
  merge, anche nel file `tests/corpus/esegui/op_<area>.cpp` di quel pacchetto.
- **Le tolleranze di un valore costruito** sono l'errore che la funzione
  dichiara nella sua intestazione (per esempio `kRelativePointError · max(M,
  |p|)`), calcolato da chi scrive il caso sul valore esatto e arrotondato per
  eccesso; `provenienza` dice come. Un valore che il contratto garantisce
  esatto (una decisione, un conteggio, un dato copiato) si scrive esatto.
- **Gli archi.** Il significato di un arco dipende dalle direzioni D0 e D1 di
  `robust::sinCos`, che ha un errore fino a 2 ulp (`geom2d/curves.hpp`). Un caso
  con un arco vale solo se la risposta non cambia per nessun D0 e D1 entro quella
  distanza dai valori esatti. I casi al limite si scrivono con i segmenti di
  polilinea, che non usano la trigonometria, o con dati in cui D0 e D1 sono lo
  stesso `double`.
- **Le scale.** I casi delle richieste di Tecnigrafo si scrivono con i dati in
  `double` come Tecnigrafo li darebbe; quelli «traslati di 1e6» o «in
  chilometri» sono casi a sé, con i loro valori.
- **La descrizione** cita la richiesta (`#20`) e, se c'è, il caso della richiesta
  o il file e le entità da cui nasce.

## Regole della tappa che valgono qui

- Il formato dell'ADR 0005 non cambia: si aggiungono un componente, una forma di
  `atteso` e un vocabolario. Se qualcosa non torna, ci si ferma e si scrive nella
  issue; decide il responsabile.
- Python 3.9 con la sola libreria standard, come il resto del validatore.
- Il pacchetto non scrive casi in `tests/corpus/casi/`: sono dei pacchetti di
  riferimenti.

## Test richiesti

- Il validatore accetta un caso con `componente` `geom2d` nella cartella
  `casi/geom2d/` e uno con nomi in `atteso`, a qualsiasi profondità.
- Il validatore rifiuta, con file e campo, una stringa che non è né frazione né
  nome (`"At start"`, `"_x"`, `"1a"`): il file
  `prove/caso-nome-non-valido.json`.
- Le righe di `in-attesa.txt` con `geom2d/<id>` sono accettate.
- `ctest -L corpus` passa sui tre sistemi.

## Fatto quando

- `geom2d` è un componente dell'archivio nel validatore e nel README.
- I nomi sono ammessi in `atteso` e le stringhe che non sono né frazioni né nomi
  sono rifiutate con un messaggio che dice file e campo.
- La sezione "Dati del piano" del README contiene il vocabolario e le regole qui
  sopra.
- La CI è verde.
