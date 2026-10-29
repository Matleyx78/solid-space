# Modelli degli ADR di valutazione

Ogni richiesta di Tecnigrafo riceve un ADR di valutazione
([ADR 0007](0007-tappe-su-richiesta.md)). Come ogni ADR, si scrive prima del
codice e non si riscrive: se la decisione cambia, un nuovo ADR sostituisce il
vecchio. Il modello generale è [0000-modello.md](0000-modello.md); qui ci sono i
due formati delle valutazioni.

Copia il formato giusto in `docs/adr/NNNN-titolo-breve.md` con il numero
successivo. Il titolo dice il bisogno, non la soluzione.

## Gli esiti

| Esito | Formato | Che cosa nasce |
|---|---|---|
| 1. OCCT basta così com'è | breve | pacchetti nella tappa, o nessuno |
| 2. OCCT va corretto o migliorato | completo, con il contratto | pacchetti nella tappa |
| 3. Serve un pezzo nostro | completo, con il contratto | pacchetti nella tappa |
| 4. Non spetta a solid-space | breve | nessun pacchetto |

I pacchetti stanno nella tappa di solid-space che corrisponde alla tappa di
Tecnigrafo da cui nasce la richiesta (ADR 0007, punto 10).

Si sceglie in quest'ordine: spetta a solid-space? basta OCCT così com'è? il
problema sta dentro ciò che la facciata può correggere? altrimenti serve un pezzo
che OCCT non ha. Sostituire un modulo di OCCT non è un esito: serve un ADR con i
dati dell'archivio e una soglia dichiarata prima (ADR 0007, punto 6).

**Quando decide un vincolo già preso** (per il 2D, l'ADR 0030 di Tecnigrafo,
punto 5, esclude OCCT da `core`), la valutazione lo cita in una o due righe
invece di rifare il confronto con OCCT, e usa lo spazio per il pezzo nostro e
per il contratto (ADR 0007, punto 8).

## Intestazione (uguale nei due formati)

```markdown
# ADR NNNN: Bisogno in breve

- **Stato:** Proposto | Accettato | Sostituito da ADR NNNN
- **Data:** AAAA-MM-GG
- **Richiesta:** #N — da Tecnigrafo: tappa X.Y, Matleyx78/tecnigrafo#N
- **Esito:** 1 OCCT basta | 2 OCCT da correggere o migliorare | 3 pezzo nostro | 4 non spetta a solid-space
- **Tappa:** X.Y ([scheda](../tappe/X.Y-nome.md)) | nessuna
```

L'esito si scrive con il numero e le parole, così un elenco di file si filtra a
colpo d'occhio.

## Formato completo (esiti 2 e 3)

```markdown
# ADR NNNN: Bisogno in breve

(intestazione sopra)

## Richiesta

Il bisogno di Tecnigrafo in tre o quattro righe, con il collegamento alla issue
`richiesta`. Il testo completo sta lì, non qui.

## Valutazione

- Che cosa fa OCCT oggi per i casi della richiesta, e su che cosa si basa
  l'affermazione: comportamento documentato, archivio dei casi, una prova
  eseguita.
- Che cosa non si è potuto verificare.
- Esito 2: dove sta il problema (quale operazione, quali casi) e quale parte si
  corregge dentro la facciata.
- Esito 3: perché OCCT non offre il pezzo e in quale componente (ADR 0002) sta.
  Se un vincolo già preso esclude OCCT, basta citarlo.

## Alternative considerate

Gli altri esiti, uno per uno, con il motivo per cui si scartano (una riga
ciascuno, se li esclude un vincolo già preso). Per l'esito 3, anche l'eventuale
uso di una libreria esistente.

## Decisione

Che cosa si fa, in frasi numerate e verificabili.

## Contratto

Le intestazioni pubbliche in bozza che stanno nella stessa pull request (ADR
0007, punto 15), con il percorso di ciascuna: che cosa dichiarano, quale errore
garantisce ogni costruzione, quali diagnosi restituiscono. Compilano senza
implementazione. Quando l'ADR è accettato diventano il contratto con Tecnigrafo:
da allora si cambiano solo con una decisione dei due progetti (etichetta
`contratto`). Se la valutazione non cambia interfacce pubbliche, si scrive
«nessuno».

## Conseguenze

Componenti toccati, interfacce pubbliche che cambiano (con Tecnigrafo), nuove
dipendenze (con il loro ADR), rischi, che cosa resta fuori dalla tappa.

## Verifica

Come si prova che il bisogno è soddisfatto: quali casi di riferimento servono
(ADR 0004), con quali valori attesi indipendenti dal programma, chi li scrive. Il
«quando è soddisfatta» della richiesta diventa un criterio di chiusura.
```

## Formato breve (esiti 1 e 4)

```markdown
# ADR NNNN: Bisogno in breve

(intestazione sopra)

## Richiesta

Il bisogno in una o due righe, con il collegamento alla issue `richiesta`.

## Valutazione

Perché si arriva a questo esito, in poche righe, e su che cosa si basa.

## Decisione

- Esito 1: che cosa espone la facciata `kernel` e come si prova (quali casi). Se
  non c'è nulla da fare perché è già disponibile, si scrive dove.
- Esito 4: a chi spetta il bisogno e che cosa Tecnigrafo deve sapere per
  procedere. La richiesta si chiude con questo testo.
```

Un esito 1 o 4 che durante la tappa si rivela sbagliato non si corregge: un nuovo
ADR lo sostituisce, con l'esito giusto.
