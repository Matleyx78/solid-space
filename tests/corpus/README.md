# Archivio dei casi di verifica

Qui stanno i casi di verifica di solid-space: piccoli problemi con il risultato
giusto già scritto, che dalla tappa 0.2 i test confrontano con il comportamento del
codice. Il metodo è nell'[ADR 0004](../../docs/adr/0004-robustezza-e-verifica.md),
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
  esterni/<nome>                  copie locali dei file esterni (facoltative)
  valida_corpus.py                il validatore
  test_valida_corpus.py           i test del validatore
  prove/                          file sbagliati di proposito, per i test del validatore
```

`<componente>` è uno tra `robust`, `naming`, `solver`, `kernel`, `features`.

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
| Booleano | `true` o `false` | `"valido": true` |
| Elenco o gruppo | array o oggetto con valori delle stesse forme, a qualsiasi profondità | `["1/2", "1/2", "1/2"]` |

- Un **numero nudo** (`"volume": 1.0`) non è ammesso: non dice se è esatto o
  approssimato. Un conteggio è un valore esatto: `"6"`, non `6`.
- `valore` e `tolleranza` sono chiavi riservate. `null` non è ammesso.
- Testo che non sia una frazione non è ancora previsto. Quando un caso ne avrà
  bisogno (per esempio gli `StableId` della genealogia, tappa 0.4) si aggiunge un
  campo al formato, come dice l'ADR 0005.
- Che cosa si confronta per ciascuna chiave (volume, area, numero di facce,
  validità, genealogia) lo stabiliscono i pacchetti che eseguono i casi, dalla
  tappa 0.2. Qui contano solo la forma e la provenienza.

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
- Una copia locale del file sta in `esterni/<nome>`. Se c'è, il validatore ne
  controlla l'hash; se manca non è un errore, perché i test non dipendono dalla
  rete e **il validatore non scarica nulla**. Una copia entra nel repository solo
  se la licenza lo permette. Il download verificato lo introduce il primo pacchetto
  che usa un file esterno, in un passo separato della CI.

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
è 0. **Non** esegue i casi contro il codice (entra con la tappa 0.2), non scarica
file e non giudica se i valori attesi siano giusti.

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

## Cambiare il formato

Il formato è un contratto (ADR 0005): un campo si aggiunge, non si cambia; per
cambiarlo serve un nuovo ADR. Se un punto di questa specifica non torna, ci si ferma
e lo si scrive nella issue: la decisione è del responsabile.
