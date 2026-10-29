# [0.2-P2] Esecuzione dei casi

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** #…
- **Difficoltà:** media
- **Agente:** codex
- **Modello e impegno:** medio; impegno medio
- **Riferimenti:** non servono: infrastruttura di prova, che non cambia il
  comportamento della libreria; la provano i suoi test
- **Dipende da:** P1
- **Sblocca in Tecnigrafo:** niente direttamente; serve a ogni pacchetto di
  lavoro della tappa
- **Componente:** nessuno (`tests/corpus/` e `tests/`)
- **Da leggere:** [ADR 0005](../../adr/0005-formato-dei-casi-di-verifica.md),
  [tests/corpus/README.md](../../../tests/corpus/README.md) (con la sezione
  "Dati del piano" di P1), `tests/CMakeLists.txt`,
  `geom2d/include/solid_space/geom2d/curves.hpp`, `diagnosis.hpp`,
  `intersection.hpp` (solo i tipi), `robust/include/solid_space/robust/diagnosis.hpp`

## File

- `tests/corpus/genera_casi.py` (nuovo)
- `tests/corpus/test_genera_casi.py` (nuovo)
- `tests/corpus/esegui/value.hpp`, `value.cpp` (nuovi)
- `tests/corpus/esegui/operations.hpp`, `operations.cpp` (nuovi)
- `tests/corpus/esegui/main.cpp` (nuovo)
- `tests/corpus/esegui/plane_data.hpp`, `plane_data.cpp` (nuovi)
- `tests/corpus/esegui/value_test.cpp` (nuovo)
- `tests/corpus/README.md` (sezione nuova "Esecuzione")
- `tests/corpus/valida_corpus.py`, solo se le sue funzioni di lettura non si
  possono importare così come sono: il minimo per renderle importabili, senza
  cambiare il comportamento del validatore
- `tests/CMakeLists.txt`

## Cosa fare

I casi diventano test GoogleTest. Il C++ non legge JSON: un generatore Python
scrive i casi in una tabella C++ al momento della compilazione (ADR 0005,
alternative: niente lettore JSON in C++).

### Il generatore (`genera_casi.py`)

`python3 genera_casi.py <cartella del corpus> <cartella di uscita>
--componenti robust geom2d`. Scrive nella cartella di uscita due file:
`casi.cpp`, la tabella, e `obbligatori.txt`. In `tests/CMakeLists.txt` la
cartella di uscita è `${CMAKE_BINARY_DIR}/corpus/`. Legge i casi dei componenti
indicati e
`in-attesa.txt` (riusa le funzioni di lettura di
`valida_corpus.py`, senza duplicarle) e scrive una tabella di casi: `id`,
`componente`, numero della issue se il caso è in attesa (0 se è obbligatorio),
`ingresso` e `atteso` come alberi di `Value`. Regole:

- in `ingresso` un numero JSON diventa il suo `double`, scritto nel C++ come
  letterale esadecimale (`float.hex`), così nessun compilatore lo riarrotonda;
  una stringa che è un letterale esadecimale C diventa lo stesso `double`; le
  altre stringhe restano testo;
- in `atteso` una frazione resta testo esatto e porta anche il `double` che la
  rappresenta, se ne esiste uno uguale; un approssimato porta `valore` e
  `tolleranza`; un nome resta un nome;
- l'ordine delle chiavi è quello del file; l'uscita non dipende dall'ordine in
  cui il sistema elenca i file (casi ordinati per componente e `id`);
- un caso non valido ferma la generazione con il messaggio del validatore;
- i casi degli altri componenti (oggi `kernel/cubo-lato-1`, il caso di prova
  del formato) non si eseguono: li porterà nell'eseguibile la tappa che realizza
  quel componente, aggiungendolo a `--componenti`.

### I valori (`value.hpp`)

```cpp
namespace solid_space::corpus {

/// A value of a case, read from its JSON (ingresso, atteso) or produced by an
/// operation (the result to compare).
class Value {
public:
    enum class Kind : std::uint8_t {
        boolean, number, exact, approximate, name, text, list, group
    };
    // Constructors for each kind, accessors with a precondition on kind(),
    // and for group the lookup by key (nullptr when the key is missing).
    static Value integer(std::int64_t value);  // an exact integer, as "3"
    ...
};

/// Compares a result with what the case expects: the empty list when they
/// agree, otherwise one line per difference with the path ("punti[1].punto[0]").
[[nodiscard]] std::vector<std::string> compare(const Value& expected, const Value& actual);

/// The canonical text of a result, with every double in hexadecimal (signed
/// zeros, NaN and infinities included), for the comparison between systems (P3).
[[nodiscard]] std::string canonicalText(const Value& actual);
}
```

Regole di `compare` (in `atteso` le forme del README; nel risultato gli stessi tipi):

- esatto contro numero: uguali solo se il testo esatto ha un `double` e il
  numero è quello (`+0` e `-0` sono uguali: dove il segno dello zero conta, la
  scheda del pacchetto aggiunge una chiave booleana); esatto contro esatto: lo
  stesso testo;
- approssimato contro numero: `|numero − valore| <= tolleranza`;
- booleano, nome: uguali;
- elenco: stessa lunghezza ed elementi uguali, nell'ordine;
- gruppo: ogni chiave di `atteso` c'è nel risultato ed è uguale; le chiavi in più
  del risultato non contano, così un caso può fermarsi a ciò che vuole provare.

### Le operazioni (`operations.hpp`)

```cpp
namespace solid_space::corpus {

/// What an operation does with the "ingresso" of a case: its result, as a
/// Value to compare with "atteso".
using Operation = Value (*)(const Value& input);

/// Registers an operation under the name used in "operazione". Each
/// tests/corpus/esegui/op_<area>.cpp holds one static Registration per
/// operation; registering the same name twice is a test failure.
class Registration {
public:
    Registration(std::string_view name, Operation operation);
};

[[nodiscard]] Operation findOperation(std::string_view name);  // nullptr if none
}
```

### I dati del piano (`plane_data.hpp`)

Lettura e scrittura del vocabolario della sezione "Dati del piano" del README,
per tutti i pacchetti della tappa (nomi C++ in inglese, chiavi JSON del
README): lettura di numero (anche `"nan"`, `"inf"`, `"-inf"` e l'esadecimale),
punto (`robust::Point2`), curva (`geom2d::Curve`), polilinea
(`geom2d::Polyline`), prolungamento (`geom2d::Extension`); scrittura verso
`Value` di punto, curva, posizione (`geom2d::CurvePosition`), diagnosi (di
`robust`, senza `dato`, e di `geom2d`) e dei nomi di `Location`, `Contact`,
`Side` e dei due `DiagnosisCode`. I nomi degli altri enumeratori (`Placement`,
`TangencyOutcome`, `OffsetOutcome`, `OffsetFailure`, `PieceOrigin`) li scrive il
file `op_<area>.cpp` del pacchetto che li usa. Usa solo i **tipi** delle
intestazioni del contratto, nessuna funzione: compila prima che le funzioni
siano realizzate. Un dato malformato fa fallire il caso con un messaggio che
dice quale chiave.

### Il programma (`main.cpp`)

Due modi, scelti dalla riga di comando.

**Modo normale** (quello di `ctest`, un processo per caso):

- un test GoogleTest per caso, con nome `Corpus.<componente>_<id>` (trattini
  sostituiti da `_`), registrato a partire dalla tabella generata;
- un caso in attesa dà `GTEST_SKIP()` con il numero della issue, prima di
  cercare la sua operazione;
- un caso obbligatorio senza operazione registrata fallisce;
- in questo modo il programma **non scrive mai** file di risultati, così i
  processi che `ctest` avvia in parallelo non si contendono nulla.

**Modo risultati** (`--ss-results=<file>`, per il confronto tra i sistemi di
P3):

- il programma non usa il runner di GoogleTest: scorre la tabella generata, in
  ordine di `<componente>/<id>` (ordine dei byte, lo stesso su ogni sistema), ed
  esegue **tutti e soli i casi obbligatori**, uno dopo l'altro, nello stesso
  processo;
- per ciascuno scrive una riga `<componente>/<id> <canonicalText>`, anche quando
  il risultato non coincide con `atteso` (il confronto con `atteso` lo fanno i
  test del modo normale; qui conta che i tre sistemi diano gli stessi bit);
- in testa scrive `# casi: N`, con N il numero dei casi obbligatori della
  tabella;
- un caso obbligatorio senza operazione, o un'operazione che non sa leggere il
  suo `ingresso`, dà una riga `<componente>/<id> ERRORE <motivo>` e l'esito del
  programma è 1;
- scrive in `<file>.tmp` e lo rinomina in `<file>` solo alla fine, se tutto è
  andato bene: un file a metà non esiste mai;
- ogni altra opzione (per esempio `--gtest_filter`, che sceglierebbe un
  sottoinsieme) insieme a `--ss-results` è un errore d'uso, con esito 2.

**Completezza.** `obbligatori.txt` elenca gli `<componente>/<id>` dei casi
obbligatori, uno per riga, nello stesso ordine del modo risultati. Il programma
non lo legge: scrive una riga per ogni caso obbligatorio della tabella per
costruzione, e un confronto con un elenco uscito dalla stessa generazione non
proverebbe nulla. Il controllo vero lo fa P3 sui file che arrivano dai tre
sistemi: un'esecuzione interrotta, ripetuta o ristretta non produce un
`risultati.txt` che corrisponda a `obbligatori.txt`.

### Compilazione (`tests/CMakeLists.txt`)

- Un comando che rigenera il `.cpp` e `obbligatori.txt` quando cambia un caso,
  `in-attesa.txt` o il generatore.
- L'eseguibile `solid_space_corpus_test`, con i file di `esegui/` e **tutti** i
  file `tests/corpus/esegui/op_*.cpp`, raccolti con
  `file(GLOB ... CONFIGURE_DEPENDS ...)`: i pacchetti successivi aggiungono il
  loro file `op_<area>.cpp` e non toccano `tests/CMakeLists.txt`.
- I test del modo normale scoperti con `gtest_discover_tests` e l'etichetta
  `corpus`.
- Un test `corpus_results`, con le etichette `corpus` e `determinismo`, che
  esegue una volta il programma con
  `--ss-results=${CMAKE_BINARY_DIR}/corpus/risultati.txt`: è l'unico processo che
  scrive quel file, quindi non ci sono scritture concorrenti né sovrascritture
  da processi diversi. `obbligatori.txt` sta nella stessa cartella.
- `test_genera_casi.py` sotto `ctest` come gli altri test Python.

### Il README

Una sezione "Esecuzione" che dice come i casi diventano test, che cosa vuol dire
uguale per ogni forma, dove si aggiunge un'operazione, e come si producono
`risultati.txt` e `obbligatori.txt` (il modo risultati e il test
`corpus_results`).

## Regole della tappa che valgono qui

- Nessuna nuova dipendenza: GoogleTest c'è già, Python con la sola libreria
  standard.
- Codice e commenti in inglese (AGENTS.md); in italiano restano le chiavi JSON
  dei casi, i nomi dei file Python accanto a `valida_corpus.py` e la cartella
  `esegui/`, come `casi/` e `prove/`.
- Il pacchetto non tocca le intestazioni del contratto e non chiama nessuna
  funzione non realizzata.
- File comuni: `tests/CMakeLists.txt` (poi lo tocca P3, che dipende da P2) e
  `tests/corpus/README.md` (dopo P1, da cui P2 dipende).

## Test richiesti

- `test_genera_casi.py`: un numero JSON e lo stesso valore in esadecimale danno
  lo stesso letterale; una frazione che è un `double` porta il `double`, una che
  non lo è no; un caso in attesa porta la sua issue; l'uscita è la stessa
  rigenerando.
- `value_test.cpp`: `compare` per ogni forma, con un caso uguale e uno
  diverso, e il percorso nel messaggio; `canonicalText` scrive i `double` in
  esadecimale, con lo zero negativo, NaN e gli infiniti.
- Il modo risultati, con una tabella di prova di pochi casi (un test del
  programma, non l'archivio): l'ordine delle righe non dipende dall'ordine dei
  casi nella tabella; un caso in attesa non compare; un caso senza operazione dà
  `ERRORE` ed esito 1 e il file non viene creato; `--gtest_filter` insieme a
  `--ss-results` dà esito 2.
- Con l'archivio di oggi, che non ha casi di `robust` e `geom2d`, l'eseguibile
  gira e passa, e `corpus_results` scrive un file con la sola riga `# casi: 0`;
  `casi/kernel/cubo-lato-1.json` non si tocca.

## Fatto quando

- Ogni caso dell'archivio è un test di `ctest`, e un caso in attesa è saltato con
  il numero della sua issue.
- Un pacchetto aggiunge un'operazione scrivendo solo il suo file
  `op_<area>.cpp`, senza toccare `tests/CMakeLists.txt`.
- Il test `corpus_results` scrive, in un solo processo, `risultati.txt` con tutti
  e soli i casi obbligatori in forma canonica e in ordine fisso, e
  `obbligatori.txt` li elenca.
- La CI è verde.
