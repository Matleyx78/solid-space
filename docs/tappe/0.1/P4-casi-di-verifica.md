# [0.1-P4] Casi di verifica

- **Tappa:** 0.1 — [piano](../0.1-fondamenta.md)
- **Issue:** [#6](https://github.com/Matleyx78/solid-space/issues/6)
- **Difficoltà:** impegnativa
- **Agente:** claude
- **Modello e impegno:** forte (Opus 5.5 alto); impegno alto
- **Riferimenti:** non servono: il pacchetto costruisce il formato in cui si
  scrivono i riferimenti, e non se ne può scrivere uno in un formato che non esiste;
  il caso di prova ha valori ricavati a mano e lo rivedono il responsabile e Gemini
- **Dipende da:** P2, ADR 0005 accettato
- **Componente:** nessuno (`tests/corpus/`)
- **Da leggere:** [ADR 0004](../../adr/0004-robustezza-e-verifica.md) (punti 6–9),
  [ADR 0005](../../adr/0005-formato-dei-casi-di-verifica.md) (decisioni del
  formato), [CODING_STYLE.md](../../CODING_STYLE.md) ("Test"), `tests/CMakeLists.txt`
  di P2

## File

- `tests/corpus/README.md`: la specifica completa del formato
- `tests/corpus/sorgenti.json` (vuoto ma valido) e `tests/corpus/in-attesa.txt`
- `tests/corpus/casi/<componente>/<nome>.json`: un caso di prova
- `tests/corpus/valida_corpus.py`: il validatore
- `tests/corpus/test_valida_corpus.py`: i test del validatore, con file volutamente
  sbagliati in `tests/corpus/prove/`
- `tests/CMakeLists.txt` (solo la riga che registra il controllo in `ctest`)

## Cosa fare

- **Specifica.** `README.md` scrive per intero il formato deciso dall'ADR 0005: i
  campi di un caso (`versione`, `id`, `componente`, `descrizione`, `ingresso`,
  `atteso`, `provenienza`), le frazioni esatte, la tolleranza, le voci di
  `sorgenti.json`, la regola di `in-attesa.txt`. Una persona o un agente deve poter
  scrivere un caso leggendo solo quel file.
- **Validatore** in Python 3, sola libreria standard: controlla la struttura come
  elencato nell'ADR 0005, punto 5, e restituisce un errore con il nome del file e
  del campo. Non scarica nulla.
- **Caso di prova**, nel formato definitivo, con valori ricavati a mano e il
  procedimento scritto in `provenienza` (per esempio un cubo di lato 1: volume 1,
  area 6, 6 facce). Non usa nessun file esterno.
- **Test del validatore**: casi buoni e casi sbagliati (campo mancante, provenienza
  assente, hash errato, riga di `in-attesa.txt` che punta a un caso inesistente).
- **Collegamento a `ctest`**: il controllo gira con `ctest`, con l'etichetta
  `corpus`.

## Regole della tappa che valgono qui

- Il validatore non cambia il formato deciso nell'ADR 0005: se qualcosa non torna,
  ci si ferma e si scrive nella issue; la decisione è del responsabile.
- Python 3 serve solo come strumento di sviluppo; nessuna libreria oltre alla
  libreria standard.
- Il pacchetto non tocca `AGENTS.md`: le regole sui riferimenti ci sono già.

## Test richiesti

- Il validatore accetta il caso di prova e `sorgenti.json` vuoto.
- Il validatore rifiuta ciascuno dei casi sbagliati elencati sopra, con un messaggio
  che dice quale file e quale campo.
- `ctest -L corpus` passa su Linux, Windows e macOS.

## Fatto quando

- Esiste almeno un caso di verifica di prova nel formato definitivo, con la
  provenienza dei suoi dati registrata.
- `tests/corpus/README.md` descrive il formato in modo sufficiente a scrivere un
  caso senza leggere altro.
- `sorgenti.json` e `in-attesa.txt` esistono, sono validi e la regola "in attesa" è
  scritta.
- Il validatore gira da `ctest` e rifiuta i file sbagliati dei test.
