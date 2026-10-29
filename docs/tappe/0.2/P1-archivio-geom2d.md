# [0.2-P1] Archivio: `geom2d`, nomi e dati del piano

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** #…
- **Difficoltà:** media
- **Agente:** codex
- **Modello e impegno:** medio; impegno medio
- **Riferimenti:** non servono: il pacchetto prepara il formato in cui si
  scrivono i riferimenti della tappa; lo provano i test del validatore e quelli
  della guardia nella CI
- **Dipende da:** — (la parte sui file esterni si fa solo se il responsabile ha
  preso la decisione 3 della sezione 5 del piano prima dell'apertura; la issue lo
  dice)
- **Sblocca in Tecnigrafo:** niente direttamente; da qui partono tutti i
  pacchetti di riferimenti della tappa
- **Componente:** nessuno (`tests/corpus/` e la guardia della CI)
- **Da leggere:** [ADR 0005](../../adr/0005-formato-dei-casi-di-verifica.md)
  (Decisione, con il punto 7 come lo avrà lasciato la decisione del
  responsabile, e Conseguenze), [ADR 0008](../../adr/0008-componente-geom2d.md)
  punto 8, [tests/corpus/README.md](../../../tests/corpus/README.md),
  `tests/corpus/valida_corpus.py` e `test_valida_corpus.py`, il job `references`
  di `.github/workflows/ci.yml`

## File

- `tests/corpus/valida_corpus.py`
- `tests/corpus/test_valida_corpus.py`
- `tests/corpus/prove/caso-nome-non-valido.json` (nuovo)
- con la decisione 3: `tests/corpus/prove/esterni-senza-voce/` e
  `tests/corpus/prove/voce-senza-copia/` (nuove: piccoli archivi di prova per i
  due controlli nuovi del validatore)
- `tests/corpus/README.md`
- con la decisione 3: `.github/workflows/ci.yml` (solo il job `references`) e
  `.gitattributes` (nuovo)

## Cosa fare

Tre aggiunte al formato, che non cambiano nessun caso esistente, e l'adeguamento
della separazione tra riferimenti e lavoro ai file esterni e la forma a blocchi
di `in-attesa.txt` (le due sezioni dopo il testo del README).

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
| numero | un numero JSON, cioè il `double` più vicino a ciò che è scritto; oppure una stringa con il `double` in esadecimale C (`"0x1.0000000000001p+0"`, `"-0x1p-1074"`) quando serve un valore preciso al bit, e sempre per lo zero negativo (`"-0x0p+0"`: il JSON non lo distingue da 0); oppure `"nan"`, `"inf"`, `"-inf"`, che il JSON non ammette |
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
| diagnosi | `{"diagnosi": {"codice": nome di DiagnosisCode, "dato": intero esatto}}` per `geom2d`; `{"diagnosi": {"codice": nome}}` per `robust`, la cui diagnosi non dice il dato. Quando la funzione intera dà una diagnosi, `atteso` ha solo questa chiave; una diagnosi dentro un elemento (per esempio un tagliente scartato da `intersectAlong`) ha la stessa forma, accanto alle altre chiavi dell'elemento |
| conteggi e indici | interi esatti (`"0"`, `"3"`) |

Regole per chi scrive i casi:

- **Le chiavi e le operazioni** di ogni funzione sono nella scheda del pacchetto
  che la realizza (`docs/tappe/0.2/`), sezione "Casi di verifica"; dopo il
  merge, anche nel file `tests/corpus/esegui/op_<area>.cpp` di quel pacchetto.
  Un caso può scrivere solo una parte delle chiavi di un risultato: si confronta
  ciò che è scritto.
- **I file esterni** (i DXF e gli script di Tecnigrafo): con la decisione 3
  hanno la loro copia in `esterni/`, perché le loro licenze (CC0,
  GPL-3.0-or-later) lo permettono, e il validatore ne controlla l'hash senza
  scaricare nulla; senza, si registrano solo le voci (sezione «I file esterni
  sono riferimenti»).
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

### I file esterni sono riferimenti

Questa parte si fa **solo con la decisione 3** della sezione 5 del piano, che
corregge l'ADR 0005 (punti 1 e 7) e la regola del README sul download verificato.

R13 e R16b aggiungono casi, voci di `sorgenti.json` e le copie dei DXF e degli
script in `tests/corpus/esterni/`, tutto nella stessa pull request (R14b usa le
copie di R13 senza toccarle). Oggi la guardia della CI (job `references`, ADR
0005, punto 7) conta `esterni/` come lavoro e respinge quella pull request. Con
la decisione 3:

- **guardia:** in `classify` i riferimenti sono `tests/corpus/casi/`,
  `tests/corpus/sorgenti.json` **e `tests/corpus/esterni/`**; il lavoro è tutto
  il resto tranne `tests/corpus/in-attesa.txt`, che resta ammesso in entrambi.
  Così una pull request di riferimenti può portare le copie, e una di lavoro
  che tocca `esterni/` è respinta come oggi una che tocca `casi/`. Le
  asserzioni in testa allo script provano i due casi nuovi: casi, voce e copia
  insieme (accettati) e una copia insieme a un file di codice (respinti),
  anche quando la copia è spostata fuori da `esterni/`;
- **validatore:** ogni file in `esterni/` deve avere la sua voce in
  `sorgenti.json` (stesso `nome`), e ogni voce la sua copia in `esterni/`; i due
  difetti sono errori con il nome del file. Così `esterni/` contiene solo copie
  registrate, e ogni file esterno ha l'hash controllato a ogni esecuzione;
- **`.gitattributes`:** `tests/corpus/esterni/** -text`, perché git non
  cambi i fine riga dei DXF e degli script su Windows: l'hash è dei byte
  originali;
- **README:** "Disposizione" e "`sorgenti.json`" dicono che le copie in
  `esterni/` sono parte dei riferimenti, si aggiungono solo con una pull request
  di riferimenti e sono obbligatorie; un file che la licenza non lascia copiare
  non entra nell'archivio finché un ADR non introduce il download verificato
  (la frase che oggi lo assegna al primo pacchetto con un file esterno cambia di
  conseguenza).

**Senza la decisione 3** questa parte non si fa: R13 e R16b registrano le voci
senza copie, come il README già permette, e nessun hash si controlla; il
download verificato che il README promette resta da fare, con un pacchetto da
aggiungere al piano (riga in "Modifiche al piano").

### `in-attesa.txt` a blocchi

Nel README, sezione "`in-attesa.txt`": in una tappa le righe di ogni pacchetto
di riferimenti stanno in un blocco preceduto dal commento `# [X.Y-RN]`,
nell'ordine dei pacchetti nel piano; il pacchetto di lavoro toglie i blocchi dei
suoi riferimenti, commento compreso. È la forma che rende sicura l'eccezione
della sezione 6 del piano (file comuni). Il validatore non cambia: già ignora i
commenti.

## Regole della tappa che valgono qui

- Il formato dell'ADR 0005 non cambia: si aggiungono un componente, una forma di
  `atteso` e un vocabolario. Se qualcosa non torna, ci si ferma e si scrive nella
  issue; decide il responsabile.
- Python 3.9 con la sola libreria standard, come il resto del validatore.
- Il pacchetto non scrive casi in `tests/corpus/casi/` né copie in `esterni/`:
  sono dei pacchetti di riferimenti.
- La parte sui file esterni si fa solo con la decisione 3 della sezione 5 del
  piano: la guardia applica l'ADR 0005, punto 7, e senza la decisione `esterni/`
  non è un riferimento. P1 non scrive negli ADR: la correzione, se il
  responsabile la decide, sta nella pull request di apertura della tappa, e la
  issue di P1 dice se c'è.
- File comuni: `tests/corpus/README.md` (poi lo tocca P2, che dipende da P1);
  `.github/workflows/ci.yml` (poi lo tocca P3, che dipende da P2).

## Test richiesti

- Il validatore accetta un caso con `componente` `geom2d` nella cartella
  `casi/geom2d/` e uno con nomi in `atteso`, a qualsiasi profondità.
- Il validatore rifiuta, con file e campo, una stringa che non è né frazione né
  nome (`"At start"`, `"_x"`, `"1a"`): il file
  `prove/caso-nome-non-valido.json`.
- Le righe di `in-attesa.txt` con `geom2d/<id>` sono accettate, anche in
  blocchi con il commento `# [0.2-R4a]`.
- Con la decisione 3: il validatore rifiuta un file in `esterni/` senza voce
  (`prove/esterni-senza-voce/`) e una voce senza copia (`prove/voce-senza-copia/`);
  le asserzioni della guardia nella CI passano, con i casi nuovi.
- `ctest -L corpus` passa sui tre sistemi.

## Fatto quando

- `geom2d` è un componente dell'archivio nel validatore e nel README.
- I nomi sono ammessi in `atteso` e le stringhe che non sono né frazioni né nomi
  sono rifiutate con un messaggio che dice file e campo.
- La sezione "Dati del piano" del README contiene il vocabolario e le regole qui
  sopra, e la sezione "`in-attesa.txt`" i blocchi.
- Con la decisione 3: la guardia della CI conta `tests/corpus/esterni/` tra i
  riferimenti, il validatore pretende una copia per ogni voce e una voce per
  ogni copia, e `.gitattributes` tiene i loro byte.
- La CI è verde.
