# [0.3-PS11] Banco delle prestazioni e stato del contratto

- **Tappa:** 0.3 — [piano](../0.3-regioni-ed-ellisse.md)
- **Issue:** #…
- **Difficoltà:** media
- **Agente:** codex
- **Modello e impegno:** medio; impegno medio. La revisione di Claude è
  leggera
- **Riferimenti:** non servono: il banco misura e non cambia il comportamento;
  il resto sono documenti e un commento
- **Contratto:** usa tutte le intestazioni della tappa; cambia solo il commento
  di stato di `robust/include/solid_space/robust/elementary.hpp`
- **Dipende da:** PS1 (#…), PS7 (#…), PS8 (#…), PS9 (#…), PS10 (#…): gli
  ultimi pacchetti di ogni catena, così ogni funzione pubblica della tappa è
  unita
- **Sblocca in Tecnigrafo:** niente
- **Componente:** prove (`tests/prestazioni/`) e documenti
- **Da leggere:** [ADR 0014](../../adr/0014-contorno-e-isole-di-una-regione.md),
  [ADR 0015](../../adr/0015-parti-di-una-curva-dentro-una-regione.md) e
  [ADR 0017](../../adr/0017-ellisse-nel-piano.md), punto 9 (gli obiettivi di
  tempo); `tests/prestazioni/README.md` e `banco.cpp` (P17 della 0.2), di cui
  segui lo schema; `docs/ARCHITETTURA.md` («Dove si trova cosa»); la sezione 6
  del [piano](../0.3-regioni-ed-ellisse.md), per sapere che i pacchetti sono
  uniti

## File

- `tests/prestazioni/banco.cpp`
- `tests/prestazioni/README.md`
- `robust/include/solid_space/robust/elementary.hpp` (solo il commento
  «Declared, not yet implemented»)
- `docs/ARCHITETTURA.md`
- `tests/corpus/README.md` (solo il rimando alle schede con il vocabolario)

La pull request porta in testa **Revisione del contratto: da fare**, con
`elementary.hpp` e il commento che cambia: è un commento di stato, ma la regola
vale per ogni cambiamento di un'intestazione del contratto
([CONTRATTI.md](../../CONTRATTI.md), «Le correzioni del contratto»).

## Cosa fare

1. **Banco**, con lo stesso schema di P17 (dati deterministici dal seme, nessuna
   libreria di benchmark, tabella Markdown, nessuna soglia che fa fallire la
   CI), righe nuove:
   - `detectBoundary`: un disegno tecnico con qualche migliaio di curve
     candidate (una griglia di segmenti, cerchi e archi raccordati) e un punto
     interno; lo stesso con un punto che dà `notEnclosed` (ADR 0014, punto 9:
     poche decine di millisecondi);
   - `clipToRegion`: qualche centinaio di righe parallele, intere, su una
     regione di qualche centinaio di pezzi con isole (ADR 0015, punto 9: pochi
     millisecondi);
   - `nestContours`: qualche centinaio di contorni annidati (nessuna soglia
     nell'ADR: misura della crescita);
   - ellisse: `intersect` con ogni tipo di curva e con un'altra ellisse, coppie
     non degeneri e quasi tangenti; `nearest` lontano e vicino all'evoluta; un
     gruppo di snap su qualche decina di entità (ADR 0017, punto 9: qualche
     decina di microsecondi per coppia, il gruppo sotto il millisecondo);
   - `tan`, `acos` e `hypot` (nessuna soglia: misura per le note).
   Il README del banco elenca le righe nuove con l'obiettivo dell'ADR accanto.
2. **`elementary.hpp`:** il commento «Declared, not yet implemented …» diventa
   il rimando alla realizzazione (tappa 0.3, PS1), come fece P18 della 0.2 per
   le sue intestazioni; nient'altro cambia.
3. **`ARCHITETTURA.md`, «Dove si trova cosa»:** la frase sulle intestazioni del
   contratto dice anche la tappa 0.3; una riga per le parti interne nuove
   (`geom2d/src/exact/polynomial.*`, l'ordine intorno a un punto e la parità,
   l'arrangiamento, le parti di una curva in una regione, il significato
   dell'ellisse).
4. **`tests/corpus/README.md`:** dove rimanda alle schede della tappa 0.2 per
   le chiavi delle operazioni, anche a quelle della tappa 0.3.

## Regole della tappa che valgono qui

- **Contratto:** solo il commento di stato di `elementary.hpp`; nessun'altra
  intestazione pubblica cambia.
- Il banco usa solo le API pubbliche e non entra in CTest; nessun
  `CMakeLists.txt` cambia (il target c'è già).
- I casi in `tests/corpus/casi/` e `in-attesa/` non si toccano.

## Test richiesti

- Il banco si compila nella CI sui tre sistemi ed esce con 0 in una prova
  locale in Release; la pull request riporta la tabella di quella prova, con
  sistema e processore, come indicazione (la misura vera la fa il
  responsabile, piano, sezione 7).

## Fatto quando

- Il banco ha le righe nuove e il suo README le descrive.
- `elementary.hpp`, `ARCHITETTURA.md` e il README dell'archivio dicono lo stato
  a fine tappa.
- La revisione del contratto della pull request è fatta.
- La CI è verde.
