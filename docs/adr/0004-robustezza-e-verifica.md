# ADR 0004: Robustezza e verifica

- **Stato:** Accettato
- **Data:** 2026-10-27
- **Corretto:** 2026-10-27 — la citazione di una tappa futura per numero (punto 10)
  è sostituita dall'evento che la fa nascere, perché le tappe del piano iniziale
  non esistono più ([ADR 0007](0007-tappe-su-richiesta.md)); lo decide il
  responsabile.

## Contesto

Il divario tra OCCT e i kernel commerciali sta nei casi limite: raccordi su
spigoli tangenti, booleane con facce coincidenti, svuotamenti sottili, geometria
importata "sporca". Un piccolo gruppo non li risolve riscrivendo il nucleo, ma può
accumulare robustezza con un metodo e misurarla. Tecnigrafo ha già un metodo
collaudato: casi di verifica scritti prima da un altro agente, con valori attesi
indipendenti dal programma (ADR 0011 di Tecnigrafo). Qui lo si estende al 3D.

## Decisione

### Comportamento delle operazioni

1. **Ogni operazione del `kernel` restituisce risultato, diagnosi e genealogia.**
   Non solleva eccezioni e non fa terminare il programma: le eccezioni di OCCT si
   fermano nella facciata e diventano una diagnosi.
2. **Controllo di validità prima di restituire.** Un risultato non valido (solido
   non chiuso, volume non coerente, controlli di OCCT negativi) non si restituisce
   come riuscito.
3. **Strategie di ripiego ordinate.** Quando un'operazione fallisce o dà un
   risultato non valido, si tenta in ordine una lista documentata di alternative
   (riparazione della forma, tolleranza di fusione nelle booleane, ordine diverso
   degli argomenti, piccole variazioni dei parametri). **Ogni strategia usata è
   riportata nella diagnosi**: mai un ripiego silenzioso.
4. **Se tutto fallisce**, l'errore dice che cosa non è riuscito e dove, con gli
   identificativi stabili degli elementi coinvolti, e l'ingresso resta
   inalterato.
5. **Esiti deterministici.** Stessi ingressi, stessi identificativi e risultati
   equivalenti sui tre sistemi operativi. L'esecuzione parallela di OCCT resta
   spenta finché non si dimostra, sull'archivio, che non cambia i risultati.

### Verifica

6. **Casi di verifica scritti prima e da un altro.** Un pacchetto che cambia il
   comportamento è finito quando i test confermano casi scritti *prima* del
   codice da un agente o da una persona diversi da chi lo scrive. Chi scrive il
   codice non modifica i casi: se ne ritiene sbagliato uno, si ferma e lo segnala.
7. **Valori attesi indipendenti dal programma.** Si ricavano da calcoli a mano, da
   aritmetica esatta o da file pubblici, **mai eseguendo solid-space**. Per i
   file presi dall'esterno si registrano indirizzo, versione, SHA-256 e licenza
   (compatibile con la GPLv3 e ridistribuibile), e i file non si copiano nel
   repository se la licenza non lo consente.
8. **L'archivio cresce.** I casi stanno in `tests/corpus/`. Ogni difetto trovato
   diventa un caso. Un caso descrive l'ingresso, il valore atteso (volume, area,
   numero di facce, validità, genealogia) e la provenienza.
9. **Predicati contro un oracolo esatto.** I predicati di `robust` si provano
   contro aritmetica razionale esatta, anche su punti quasi allineati e quasi
   cocircolari.
10. **Strumenti automatici.** AddressSanitizer e UBSan in CI; fuzzing dell'import
    STEP dalla tappa che porta l'import STEP, come Tecnigrafo fa per il DXF.

### Cambiamenti

11. **OCCT a una versione fissata.** Un aggiornamento è una pull request che
    riesegue l'archivio e ne confronta i risultati.
12. **Sostituire un modulo di OCCT** richiede un ADR con i dati dell'archivio e una
    soglia dichiarata *prima* della prova (appendice "Non ora" di
    [VISIONE.md](../VISIONE.md)).

## Alternative considerate

- **Riscrivere subito i moduli deboli di OCCT**: non si sa che cosa sia davvero
  debole né se la riscrittura sarebbe migliore.
- **Solo test scritti da chi scrive il codice**: registrano il comportamento
  attuale, errori compresi.
- **Ripiego silenzioso**: più comodo per l'utente, ma impedisce di sapere che
  cosa funziona e che cosa no.

## Conseguenze

- L'archivio è anche lo strumento per confrontare OCCT con altre strade, e può
  servire a Tecnigrafo come prova pratica.
- Servono casi con valori attesi calcolati con cura: scriverli costa, ma è il
  lavoro che gli agenti possono fare in parallelo.
- Le strategie di ripiego sono parte dell'interfaccia osservabile del `kernel`: si
  cambiano con attenzione.
