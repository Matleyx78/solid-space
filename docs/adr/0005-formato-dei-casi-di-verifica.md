# ADR 0005: Formato dei casi di verifica e strumenti di controllo

- **Stato:** Accettato
- **Data:** 2026-10-27
- **Corretto:** 2026-10-27 — le citazioni di una tappa futura per numero (contesto
  e punto 5) sono sostituite dall'evento che la fa nascere, perché le tappe del
  piano iniziale non esistono più ([ADR 0007](0007-tappe-su-richiesta.md)); lo
  decide il responsabile.

## Contesto

L'[ADR 0004](0004-robustezza-e-verifica.md) vuole un archivio di casi in
`tests/corpus/`, scritti prima del codice da qualcuno diverso da chi lo scrive, con
valori attesi indipendenti dal programma e con la provenienza dei file esterni
registrata. Non dice come è fatto un caso, dove sta l'elenco delle fonti esterne,
che cosa significa "in attesa" né con quale strumento si controlla il tutto.

La tappa 0.1 non ha codice geometrico da eseguire: i casi si possono definire e
validare, non ancora confrontare con un risultato. Il confronto arriva con la
prima tappa che porta codice geometrico da provare.

## Decisione

1. **Disposizione.** Sotto `tests/corpus/`:
   - `casi/<componente>/<nome>.json`: un caso per file;
   - `sorgenti.json`: i file presi dall'esterno;
   - `in-attesa.txt`: i casi non ancora obbligatori;
   - `README.md`: la specifica completa del formato (la scrive P4).
2. **Un caso** è un oggetto JSON con questi campi: `versione` del formato (1),
   `id` (uguale al nome del file senza estensione), `componente`, `descrizione`,
   `ingresso`, `atteso` e `provenienza`. I valori attesi esatti si scrivono come
   frazioni in stringa (`"1/3"`); quelli approssimati come numero con la
   tolleranza dichiarata nel caso. `provenienza` dice se il valore viene da un
   calcolo a mano (con il procedimento), da aritmetica esatta o da un file
   pubblico (con la chiave della voce in `sorgenti.json`). Un caso senza
   provenienza non è valido.
3. **`sorgenti.json`** elenca, per ogni file preso dall'esterno: nome, indirizzo,
   versione, hash SHA-256 e licenza (identificatore SPDX). Solo licenze compatibili
   con la GPLv3 e che permettono la ridistribuzione; se non è così, il file non
   entra nel repository.
4. **Casi in attesa.** `in-attesa.txt` ha un caso per riga, con il numero della
   issue del pacchetto di lavoro che lo renderà obbligatorio. Un caso in attesa
   non fa fallire i test. Chi fa il lavoro toglie le righe del proprio pacchetto:
   da quel momento il caso è obbligatorio.
5. **In 0.1 il validatore controlla solo la struttura:** campi presenti e di tipo
   giusto, provenienza dichiarata, voci di `sorgenti.json` complete, hash dei file
   scaricati già presenti in locale, righe di `in-attesa.txt` che puntano a casi
   esistenti. Non scarica nulla: i test non dipendono dalla rete. L'esecuzione dei
   casi contro il codice entra con la prima tappa che porta codice geometrico da
   provare.
6. **Strumento.** Il validatore è uno script **Python 3 con sola libreria
   standard**, eseguito da `ctest`. Python è uno strumento di sviluppo, non una
   dipendenza della libreria: chi usa solid-space non ne ha bisogno.
7. **Un pacchetto scrive riferimenti oppure fa il lavoro, mai entrambi.** Sono
   riferimenti `tests/corpus/casi/` e `tests/corpus/sorgenti.json`;
   `in-attesa.txt` può cambiare in entrambi i tipi di pacchetto. Lo controlla un
   job della CI (pacchetto P3 della tappa 0.1).

## Alternative considerate

- **Validatore in C++ con un parser JSON:** una dipendenza in più per uno strumento
  che non è la libreria.
- **Formato a righe di testo, senza JSON:** più semplice, ma povero per valori
  strutturati (punti, facce, genealogia) e per le frazioni esatte.
- **Validatore con download dei file esterni in ogni esecuzione:** rende i test
  dipendenti dalla rete. Il download verificato si introduce con il primo file
  esterno, in un passo separato della CI.

## Conseguenze

- L'oracolo ad aritmetica razionale esatta dell'ADR 0004 (punto 9) è a portata di
  mano con la libreria `fractions` di Python, senza dipendenze.
- Chi sviluppa ha bisogno di Python 3, e lo dice [AMBIENTE-SVILUPPO.md](../AMBIENTE-SVILUPPO.md).
- Il formato è un contratto: un campo si aggiunge, non si cambia; per cambiarlo
  serve un nuovo ADR.
- La separazione tra riferimenti e lavoro diventa verificabile dalla CI.
