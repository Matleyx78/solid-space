# Archivio dei casi di verifica

Qui stanno i casi di verifica di solid-space: piccoli problemi con il risultato
giusto già scritto, che dalla prima tappa con codice geometrico da provare i test
confrontano con il comportamento del codice. Il metodo è nell'[ADR 0004](../../docs/adr/0004-robustezza-e-verifica.md),
le decisioni sul formato nell'[ADR 0005](../../docs/adr/0005-formato-dei-casi-di-verifica.md).
Questo file è la **specifica completa del formato, versione 1**: per scrivere un
caso non serve leggere altro.

Due regole valgono sempre.

- **I valori attesi non si ricavano mai eseguendo solid-space**: vengono da un
  calcolo a mano, da aritmetica esatta o da un file pubblico. Altrimenti il caso
  registrerebbe il comportamento del programma, errori compresi.
- **Chi scrive i casi non è chi scrive il codice** che deve superarli. Un pacchetto
  di lavoro non modifica i casi che non sono suoi.

## Disposizione

```text
tests/corpus/
  README.md                       questa specifica
  casi/<componente>/<id>.json     un caso per file
  sorgenti.json                   i file presi dall'esterno
  in-attesa.txt                   i casi non ancora obbligatori
  esterni/<nome>                  copie obbligatorie dei file esterni (riferimenti)
  valida_corpus.py                il validatore
  test_valida_corpus.py           i test del validatore
  prove/                          file sbagliati di proposito, per i test del validatore
```

`<componente>` è uno tra `robust`, `geom2d`, `naming`, `solver`, `kernel`, `features`.

Le copie in `esterni/`, come i casi e `sorgenti.json`, sono riferimenti:
si aggiungono o modificano solo in una pull request di riferimenti.

## Un caso

Un file JSON (UTF-8), senza chiavi ripetute, che è un oggetto con **tutti e soli**
questi campi:

| Campo | Tipo | Contenuto |
|---|---|---|
| `versione` | intero | sempre `1` |
| `id` | stringa | uguale al nome del file senza `.json`; parole minuscole e cifre unite da trattini (`cubo-lato-1`) |
| `componente` | stringa | il componente provato; uguale alla cartella in cui sta il file |
| `descrizione` | stringa | che cosa prova il caso, in una o due frasi; non vuota |
| `ingresso` | oggetto | ciò che si dà al programma; non vuoto |
| `atteso` | oggetto | ciò che ci si aspetta, valore per valore; non vuoto |
| `provenienza` | oggetto | da dove vengono i valori attesi; senza, il caso non è valido |

Esempio completo: [`casi/kernel/cubo-lato-1.json`](casi/kernel/cubo-lato-1.json).

### `ingresso`

Struttura libera, senza `null`. Il vocabolario (nomi delle operazioni e dei
parametri) lo fissa il pacchetto del componente che per primo usa il caso, non
questo formato. I numeri JSON sono i `double` che il programma riceve, esattamente
come scritti.

### `atteso`

Ogni valore ha una di queste forme:

| Forma | Come si scrive | Esempio |
|---|---|---|
| Esatto | stringa con una frazione `p/q` (`q` diverso da zero) o un intero; niente decimali né zeri iniziali | `"6"`, `"1/2"`, `"-7/3"` |
| Approssimato | oggetto con esattamente `valore` (numero) e `tolleranza` (numero maggiore di zero, **assoluta**: il risultato è giusto se differisce da `valore` per al più `tolleranza`) | `{ "valore": 1.7320508075688772, "tolleranza": 1e-12 }` |
| Nome | stringa ASCII che rispetta `[a-z][A-Za-z0-9]*`, nome di un enumeratore del contratto | `"crossing"`, `"atStart"`, `"left"`, `"done"` |
| Booleano | `true` o `false` | `"valido": true` |
| Elenco o gruppo | array o oggetto con valori delle stesse forme, a qualsiasi profondità | `["1/2", "1/2", "1/2"]` |

- Un **numero nudo** (`"volume": 1.0`) non è ammesso: non dice se è esatto o
  approssimato. Un conteggio è un valore esatto: `"6"`, non `6`.
- `valore` e `tolleranza` sono chiavi riservate. `null` non è ammesso.
- Una stringa deve essere una frazione oppure un nome: le forme non si
  confondono, perché una frazione comincia con una cifra o con `-`. Altro testo
  (`"At start"`, `"_x"`, `"1a"`) non è ammesso.
- Che cosa si confronta per ciascuna chiave (volume, area, numero di facce,
  validità, genealogia) lo stabiliscono i pacchetti che eseguono i casi, dalla
  prima tappa con codice geometrico da provare. Qui contano solo la forma e la
  provenienza.

### `provenienza`

Oggetto con `tipo` e la chiave che lo documenta:

| `tipo` | Chiave obbligatoria | Significato |
|---|---|---|
| `calcolo-a-mano` | `procedimento` | il valore viene da un calcolo a mano; la stringa lo scrive passo per passo, così un'altra persona lo può rifare |
| `aritmetica-esatta` | `procedimento` | il valore viene da aritmetica razionale esatta (per esempio `fractions` di Python); la stringa dice quali formule e quali dati |
| `file-pubblico` | `sorgente` | il valore viene da un file pubblico; `sorgente` è la chiave di una voce di `sorgenti.json`, che deve esistere |

`procedimento` non può essere vuoto; con `file-pubblico` è facoltativo e può
dire quale parte del file si è usata. `sorgente` è ammessa solo con
`file-pubblico`. Altre chiavi non sono ammesse.

## Dati del piano (`robust` e `geom2d`)

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
  che la realizza ([schede della tappa 0.2](../../docs/tappe/0.2/)), sezione "Casi di verifica"; dopo il
  merge, anche nel file `tests/corpus/esegui/op_<area>.cpp` di quel pacchetto.
  Un caso può scrivere solo una parte delle chiavi di un risultato: si confronta
  ciò che è scritto.
- **I file esterni** (i DXF e gli script di Tecnigrafo) hanno la loro copia in
  `esterni/`, obbligatoria (ADR 0005, punto 3), e il validatore ne controlla
  l'hash senza scaricare nulla.
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

## `sorgenti.json`

Elenca i file presi dall'esterno. Senza file esterni è vuoto ma valido:

```json
{ "versione": 1, "sorgenti": {} }
```

Ogni voce di `sorgenti` ha per chiave parole minuscole e cifre unite da trattini,
e **tutti e soli** questi campi:

| Campo | Contenuto |
|---|---|
| `nome` | nome semplice del file (lettere, cifre, `.`, `_`, `-`, senza percorsi); diverso in ogni voce |
| `indirizzo` | indirizzo `https://` da cui si scarica |
| `versione` | versione, etichetta o commit del file o della sua fonte; non vuota |
| `sha256` | hash SHA-256 del file: 64 cifre esadecimali minuscole |
| `licenza` | identificatore SPDX (`CC0-1.0`, `MIT`, ...); non `NOASSERTION` né `NONE` |

- Solo licenze **compatibili con la GPLv3** e che permettono la ridistribuzione.
  Nel dubbio il file non si usa e lo si scrive nella pull request. Il validatore
  controlla la forma della licenza, non la compatibilità: la verifica chi rivede.
- Ogni voce ha una copia obbligatoria in `esterni/<nome>`, e ogni file in
  `esterni/` ha una voce con lo stesso `nome`. Il validatore rifiuta entrambe le
  mancanze e controlla l'hash dei byte, senza scaricare nulla. Le copie sono
  riferimenti e si aggiungono solo con una pull request di riferimenti, se la
  licenza permette la ridistribuzione. Git ne conserva i byte anche su Windows
  (`.gitattributes`). Il download verificato si introduce solo per il primo file
  esterno che non conviene copiare, in un passo separato della CI.

## `in-attesa.txt`

Elenca i casi già scritti ma non ancora obbligatori. Un caso in attesa non fa
fallire i test. Una riga per caso:

```text
<componente>/<id> #<numero della issue>
```

Per esempio `kernel/cubo-lato-1 #42`: il caso `casi/kernel/cubo-lato-1.json` diventa
obbligatorio con il pacchetto di lavoro della issue 42. **Chi fa quel pacchetto
toglie la riga**: da quel momento il caso è obbligatorio. Le righe vuote e quelle
che cominciano con `#` sono ignorate. Ogni riga deve puntare a un caso esistente e
nessun caso compare due volte.

In una tappa ogni pacchetto di riferimenti aggiunge un blocco preceduto dal
commento `# [X.Y-RN]`, nell'ordine dei pacchetti nel piano. Il pacchetto di
lavoro toglie i blocchi dei propri riferimenti, commento compreso; ognuno tocca
solo i propri blocchi e chi arriva dopo si aggiorna (LAVORO-IN-PARALLELO,
"Preparare il piano"). Per esempio:

```text
# [0.2-R4a]
robust/orientamento-collineare #40
```

## Il validatore

```sh
python3 tests/corpus/valida_corpus.py            # tutto l'archivio
ctest --preset release -L corpus                 # dopo aver configurato e compilato
```

Serve Python 3.9 o successivo e solo la sua libreria standard: è uno strumento di
sviluppo, non una dipendenza della libreria. In `ctest` il controllo dell'archivio è
`corpus_validate` e i test del validatore sono `corpus_validator_tests`, entrambi
con l'etichetta `corpus`.

Controlla la **struttura**: i campi dei casi, di `sorgenti.json` e di
`in-attesa.txt`, il loro tipo, la provenienza, l'hash delle copie locali e i
riferimenti (la `sorgente` di un caso, i casi di `in-attesa.txt`). Ogni errore è
una riga `file: campo: motivo` su stderr e l'esito è 1; con l'archivio valido l'esito
è 0. **Non** esegue i casi contro il codice (entra con la prima tappa con codice
geometrico da provare), non scarica file e non giudica se i valori attesi siano giusti.

I file in `prove/` sono sbagliati di proposito, uno per difetto: li usano i test del
validatore e il validatore non li guarda.

## Scrivere un caso

1. Scegli componente e `id` e crea `casi/<componente>/<id>.json`.
2. Ricava i valori attesi a mano, con aritmetica esatta o da un file pubblico
   (registrato in `sorgenti.json`), mai eseguendo solid-space.
3. Scrivi in `provenienza` il procedimento, in modo che un'altra persona lo ripeta.
4. Aggiungi i casi limite: valori nulli, punti coincidenti o quasi allineati, facce
   tangenti, spessori minimi.
5. Se il codice che lo supera non c'è ancora, aggiungi la riga in `in-attesa.txt`.
6. Esegui il validatore. Una pull request di riferimenti tocca solo
   `tests/corpus/` (ADR 0005, punto 7).

## Casi che arrivano da Tecnigrafo

L'archivio è la **verità geometrica** dei due progetti
([ADR 0007](../../docs/adr/0007-tappe-su-richiesta.md), punto 17): i calcoli
delicati si fanno una volta sola, qui, e i riferimenti di Tecnigrafo citano questi
casi invece di rifarli.

- **Da dove arrivano.** Dai «casi concreti» delle issue `richiesta` e dai file
  DXF di prova che Tecnigrafo allega al piano di una sua tappa, spesso salvati da
  AutoCAD. Entrano qui con un pacchetto di riferimenti della tappa di solid-space,
  come ogni altro caso: valori attesi indipendenti da solid-space e da
  Tecnigrafo, provenienza scritta.
- **I file DXF.** Un DXF preso da Tecnigrafo è un file esterno: ha la sua voce in
  `sorgenti.json`, con l'indirizzo del file nel repository di Tecnigrafo **a un
  commit preciso** (non `main`), quel commit come `versione`, l'hash e la licenza.
  I valori letti dal file (coordinate, punti calcolati da AutoCAD) hanno
  provenienza `file-pubblico`; quelli ricavati a mano dagli stessi dati hanno
  `calcolo-a-mano` o `aritmetica-esatta`.
- **Il nome.** L'`id` dice che cosa prova, non da dove viene; la `descrizione`
  cita la richiesta (`#N`) e, se c'è, il file e le entità di Tecnigrafo da cui
  nasce.
- **Il componente.** Il caso sta nella cartella del componente che lo deve
  superare. Un componente nuovo entra nell'elenco qui sopra con l'ADR che lo
  introduce.

### Disaccordi con Tecnigrafo

Se un riferimento di Tecnigrafo e un caso di questo archivio danno valori diversi
per la stessa geometria:

1. si apre una issue **in solid-space**, con i due valori, il caso e il
   riferimento di Tecnigrafo citati in forma assoluta
   (`Matleyx78/tecnigrafo#N` o il percorso del file a un commit);
2. **decide l'archivio**: Tecnigrafo allinea il suo riferimento al caso;
3. se è il caso a essere sbagliato, lo corregge un pacchetto di riferimenti di
   solid-space, scritto da un agente diverso da chi ha scritto il caso, e la
   issue lo cita. Il codice non si adatta mai al valore che si ritiene sbagliato
   (AGENTS.md, "In un pacchetto di lavoro").

Chi lavora a solid-space non scrive in Tecnigrafo per allineare il riferimento:
lo segnala al responsabile.

## Cambiare il formato

Il formato è un contratto (ADR 0005): un campo si aggiunge, non si cambia; per
cambiarlo serve un nuovo ADR. Se un punto di questa specifica non torna, ci si ferma
e lo si scrive nella issue: la decisione è del responsabile.
