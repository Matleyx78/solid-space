# Richiesta di Tecnigrafo a solid-space

Come Tecnigrafo chiede a solid-space ciò che gli serve ([ADR
0007](../adr/0007-tappe-su-richiesta.md)). La richiesta dice **di che cosa c'è
bisogno**, non come soddisfarlo: se basta OCCT così com'è, se va corretto o
migliorato, se serve un pezzo nostro o se il bisogno non spetta a solid-space lo
decide solid-space, con un ADR di valutazione
([modelli](../adr/0000-modello-valutazione.md)).

La richiesta nasce e vive qui, in una sola issue di solid-space con etichetta
`richiesta`, scritta con il modulo `.github/ISSUE_TEMPLATE/richiesta.yml`
(**Issues** → **New issue** → **Richiesta di Tecnigrafo**). La apre il
responsabile, oppure un agente di Tecnigrafo a cui lui lo chiede, con lo script
delle richieste del metodo comune; in Tecnigrafo non ne resta una copia, e il
piano di Tecnigrafo cita questa issue. Finché non c'è la issue qui nessuno valuta
nulla. Il workflow `controlla-richiesta.yml` commenta la issue se manca un campo o
se un riferimento non è assoluto.

## Campi

1. **Titolo.** «Richiesta: » seguito dal bisogno in una frase, senza dire come
   soddisfarlo.
2. **Da dove nasce.** La tappa di Tecnigrafo e la issue, la pull request o la
   discussione in cui il bisogno è emerso (di solito la pull request del piano
   della tappa, o la issue del pacchetto che si è fermato), scritte come
   `Matleyx78/tecnigrafo#N`: così GitHub mostra il collegamento anche dal lato di
   Tecnigrafo. Il bisogno può emergere quando Tecnigrafo prepara il piano di una
   tappa oppure mentre la percorre.
3. **Il bisogno.** Che cosa Tecnigrafo non riesce a fare o fa male, descritto da
   chi usa la libreria: che cosa vuole ottenere e che cosa succede oggi.
4. **Casi concreti.** Situazioni reali in cui il bisogno si presenta: ingressi,
   file, sequenze di operazioni, con il risultato che oggi non va. Orientano la
   valutazione; non sono i casi di verifica, che scrive il pacchetto di
   riferimenti ([ADR 0004](../adr/0004-robustezza-e-verifica.md)).
5. **Quando è soddisfatta.** Una condizione che Tecnigrafo può osservare. Diventa
   un criterio di chiusura della tappa.
6. **Vincoli.** Ciò che la soluzione deve rispettare: robustezza richiesta,
   prestazioni, sistemi operativi, dipendenze che Tecnigrafo non può avere (per
   esempio compilare senza OCCT).
7. **Che cosa blocca.** Che cosa non può andare avanti in Tecnigrafo senza questa
   richiesta e da quando. Serve al responsabile per ordinare la coda.

I sette campi sono obbligatori. Ogni riferimento a una issue o a una pull request
si scrive in forma assoluta, `Matleyx78/tecnigrafo#N` o
`Matleyx78/solid-space#N`: un `#N` da solo, in una issue di solid-space, punta a
solid-space anche quando si voleva Tecnigrafo.

## Che cosa non si scrive

- La soluzione: né «usare OCCT» né «scrivere un nostro algoritmo».
- L'interfaccia desiderata (nomi, firme, tipi). Se Tecnigrafo ha già un
  vincolo di interfaccia, si scrive tra i vincoli come fatto, non come proposta.
- I valori attesi dei casi di verifica.

## Esempio (inventato, per mostrare il modello)

> **Richiesta: decidere in modo coerente da che parte sta un punto rispetto a
> un segmento**
>
> **Da dove nasce:** tappa 0.7 di Tecnigrafo (comandi di modifica),
> Matleyx78/tecnigrafo#… sul taglio di entità.
>
> **Il bisogno:** il comando di taglio decide se due entità si intersecano e da
> che parte di un segmento sta il pezzo da tenere. Con punti quasi allineati le
> due decisioni si contraddicono: una dice che i segmenti si attraversano,
> l'altra che stanno dalla stessa parte, e il taglio tiene il pezzo sbagliato o
> dice che non c'è nulla da tagliare. Inoltre lo stesso disegno dà risultati
> diversi su Windows, Linux e macOS.
>
> **Casi concreti:** disegno `taglio-01.dxf`, entità 12 e 15; tre punti
> allineati a meno di 1e-12.
>
> **Quando è soddisfatta:** il taglio tiene sempre il pezzo giusto, anche su
> punti quasi allineati, e la CI dà lo stesso risultato sui tre sistemi
> operativi.
>
> **Vincoli:** nessuna dipendenza dal 3D né da OCCT; compila su Windows, Linux e
> macOS.
>
> **Che cosa blocca:** la tappa 0.7 di Tecnigrafo non può chiudere i comandi di
> taglio e prolungamento.

## Dopo la richiesta

1. Il responsabile, con un agente, fa scrivere l'ADR di valutazione; con gli
   esiti 2 e 3 la stessa pull request porta il contratto, le intestazioni
   pubbliche in bozza su cui Tecnigrafo scriverà le sue schede.
2. Se l'esito è 1, 2 o 3 la richiesta entra nella tappa di solid-space che
   corrisponde alla sua tappa di Tecnigrafo; se è 4, la richiesta si chiude con
   la motivazione. Una richiesta ampia riceve gli obiettivi raggiungibili, e la
   valutazione dice che cosa resta fuori.
3. I casi concreti diventano casi di verifica in `tests/corpus/`, che è la
   verità geometrica dei due progetti.
4. Quando i pacchetti della richiesta sono uniti, e al più tardi alla chiusura
   della tappa, il responsabile controlla dal lato di Tecnigrafo il «quando è
   soddisfatta» e chiude la richiesta.
