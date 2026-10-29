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
- `tests/corpus/esegui/valore.hpp`, `valore.cpp` (nuovi)
- `tests/corpus/esegui/operazioni.hpp`, `operazioni.cpp` (nuovi)
- `tests/corpus/esegui/principale.cpp` (nuovo)
- `tests/corpus/esegui/dati.hpp`, `dati.cpp` (nuovi)
- `tests/corpus/esegui/valore_test.cpp` (nuovo)
- `tests/corpus/README.md` (sezione nuova "Esecuzione")
- `tests/CMakeLists.txt`

## Cosa fare

I casi diventano test GoogleTest. Il C++ non legge JSON: un generatore Python
scrive i casi in una tabella C++ al momento della compilazione (ADR 0005,
alternative: niente lettore JSON in C++).

### Il generatore (`genera_casi.py`)

`python3 genera_casi.py <cartella del corpus> <file .cpp da scrivere>
--componenti robust geom2d`. Legge i casi dei componenti indicati e
`in-attesa.txt` (riusa le funzioni di lettura di
`valida_corpus.py`, senza duplicarle) e scrive una tabella di `Caso`: `id`,
`componente`, numero della issue se il caso è in attesa (0 se è obbligatorio),
`ingresso` e `atteso` come alberi di `Valore`. Regole:

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

### I valori (`valore.hpp`)

```cpp
namespace solid_space::corpus {

/// A value of a case, read from its JSON (ingresso, atteso) or produced by an
/// operation (the result to compare).
class Valore {
public:
    enum class Tipo : std::uint8_t {
        booleano, numero, esatto, approssimato, nome, testo, elenco, gruppo
    };
    // Constructors for each kind, accessors with a precondition on tipo(),
    // and for gruppo the lookup by key (nullptr when the key is missing).
    static Valore intero(std::int64_t value);  // an exact integer, as "3"
    ...
};

/// Compares a result with what the case expects: the empty list when they
/// agree, otherwise one line per difference with the path ("punti[1].punto[0]").
[[nodiscard]] std::vector<std::string> confronta(const Valore& atteso, const Valore& ottenuto);

/// The canonical text of a result, with every double in hexadecimal, for the
/// comparison between systems (P3).
[[nodiscard]] std::string testoCanonico(const Valore& ottenuto);
}
```

Regole di `confronta`:

- esatto contro numero: uguali solo se il testo esatto ha un `double` e il
  numero è quello (`+0` e `-0` sono uguali); esatto contro esatto: lo stesso
  testo;
- approssimato contro numero: `|numero − valore| <= tolleranza`;
- booleano, nome: uguali;
- elenco: stessa lunghezza ed elementi uguali, nell'ordine;
- gruppo: ogni chiave di `atteso` c'è nel risultato ed è uguale; le chiavi in più
  del risultato non contano, così un caso può fermarsi a ciò che vuole provare.

### Le operazioni (`operazioni.hpp`)

```cpp
namespace solid_space::corpus {

/// What an operation does with the ingresso of a case: its result, as a
/// Valore to compare with atteso.
using Operazione = Valore (*)(const Valore& ingresso);

/// Registers an operation under the name used in "operazione". Each
/// tests/corpus/esegui/op_<area>.cpp holds one static Registrazione per
/// operation; registering the same name twice is a test failure.
class Registrazione {
public:
    Registrazione(std::string_view nome, Operazione operazione);
};

[[nodiscard]] Operazione trovaOperazione(std::string_view nome);  // nullptr if none
}
```

### I dati del piano (`dati.hpp`)

Lettura e scrittura del vocabolario della sezione "Dati del piano" del README,
per tutti i pacchetti della tappa: `numero`, `punto` (`robust::Point2`),
`curva` (`geom2d::Curve`), `polilinea` (`geom2d::Polyline`), `prolungamento`
(`geom2d::Extension`); e verso `Valore`: `punto`, `curva`, `posizione`
(`geom2d::CurvePosition`), `diagnosi` (di `robust` e di `geom2d`) e i nomi di
`Location`, `Contact`, `Side` e dei due `DiagnosisCode`. Usa solo i **tipi** delle
intestazioni del contratto, nessuna funzione: compila prima che le funzioni
siano realizzate. Un dato malformato fa fallire il caso con un messaggio che
dice quale chiave.

### Il programma (`principale.cpp`)

- Un test GoogleTest per caso, con nome `Corpus.<componente>_<id>` (trattini
  sostituiti da `_`), registrato a partire dalla tabella generata.
- Un caso in attesa dà `GTEST_SKIP()` con il numero della issue, prima di
  cercare la sua operazione.
- Un caso obbligatorio senza operazione registrata fallisce.
- Se la variabile d'ambiente `SS_RISULTATI` contiene un percorso, il programma
  scrive in quel file una riga per caso eseguito,
  `<componente>/<id> <testoCanonico>`, in ordine di nome (serve a P3).

### Compilazione (`tests/CMakeLists.txt`)

Un comando che rigenera il `.cpp` quando cambia un caso, `in-attesa.txt` o il
generatore; l'eseguibile `solid_space_corpus_test`, con i file di `esegui/` e i
file `op_*.cpp` che i pacchetti successivi aggiungono; i test scoperti con
`gtest_discover_tests` e l'etichetta `corpus`; `test_genera_casi.py` sotto
`ctest` come gli altri test Python.

### Il README

Una sezione "Esecuzione" che dice come i casi diventano test, che cosa vuol dire
uguale per ogni forma, dove si aggiunge un'operazione, e che `SS_RISULTATI`
scrive i risultati.

## Regole della tappa che valgono qui

- Nessuna nuova dipendenza: GoogleTest c'è già, Python con la sola libreria
  standard.
- Il pacchetto non tocca le intestazioni del contratto e non chiama nessuna
  funzione non realizzata.
- File comuni: `tests/CMakeLists.txt` (solo le tue righe) e
  `tests/corpus/README.md` (dopo P1).

## Test richiesti

- `test_genera_casi.py`: un numero JSON e lo stesso valore in esadecimale danno
  lo stesso letterale; una frazione che è un `double` porta il `double`, una che
  non lo è no; un caso in attesa porta la sua issue; l'uscita è la stessa
  rigenerando.
- `valore_test.cpp`: `confronta` per ogni forma, con un caso uguale e uno
  diverso, e il percorso nel messaggio; `testoCanonico` scrive i `double` in
  esadecimale.
- Con l'archivio di oggi, che non ha casi di `robust` e `geom2d`, l'eseguibile
  gira e passa; `casi/kernel/cubo-lato-1.json` non si tocca.

## Fatto quando

- Ogni caso dell'archivio è un test di `ctest`, e un caso in attesa è saltato con
  il numero della sua issue.
- Un pacchetto aggiunge un'operazione scrivendo solo il suo file `op_<area>.cpp`
  e una riga in `tests/CMakeLists.txt`.
- `SS_RISULTATI` scrive i risultati in forma canonica.
- La CI è verde.
