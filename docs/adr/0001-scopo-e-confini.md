# ADR 0001: Scopo e confini

- **Stato:** Accettato
- **Data:** 2026-10-27

## Contesto

solid-space è nato come manifesto per un kernel B-Rep e un solver variazionale
liberi di classe industriale, con l'obiettivo di colmare il divario con Parasolid
+ D-Cubed e ACIS: topologia cellulare, motore di raccordi proprio, tolleranze
rigide, scomposizione a grafo dei vincoli.

Nel frattempo Tecnigrafo, il CAD libero dello stesso responsabile, ha chiarito
che cosa gli serve: prima predicati robusti per il 2D, poi vincoli parametrici 2D,
poi nella fase 2 un modellatore 3D. La sua analisi iniziale
(`docs/VALUTAZIONI-INIZIALI.md`) osserva che il vantaggio dei motori commerciali
sono decenni di casi limite risolti e che raggiungerli non è realistico per un
piccolo gruppo; è realistico costruire un'ottima applicazione sopra OCCT e
migliorarne i punti deboli con un metodo. La discussione #104 di Tecnigrafo
(OCCT o un nucleo proprio) e la #217 (spunti da solid-space) vanno nella stessa
direzione.

I due progetti hanno lo stesso responsabile.

## Decisione

1. **solid-space è il kernel geometrico di Tecnigrafo**: una libreria C++20,
   GPLv3 o successiva, scritta perché Tecnigrafo la usi. Resta una libreria
   autonoma e riusabile, ma le sue priorità le fissa la roadmap di Tecnigrafo.
2. **Tre pilastri**: solver variazionale, naming topologico stabile, operazioni
   solide robuste. Il significato concreto è in [VISIONE.md](../VISIONE.md).
3. **Si cresce a tappe piccole**, nell'ordine in cui Tecnigrafo ha bisogno dei
   pezzi, e ogni tappa ha criteri di chiusura verificabili
   ([ROADMAP.md](../ROADMAP.md)).
4. **OCCT si usa, non si riscrive.** Un suo modulo si sostituisce solo con un ADR
   che porta le misure del nostro archivio di casi e una soglia dichiarata in
   anticipo.
5. **Fuori dal piano**, finché un dato non le riporta dentro (appendice "Non
   ora" di VISIONE.md): topologia cellulare, struttura coedge propria, motore di
   raccordi proprio, tolleranze rigide, scomposizione a grafo e matrici sparse,
   disuguaglianze, nucleo alternativo.
6. **Non è un obiettivo la parità con Parasolid o ACIS.**
7. **Governo.** Stesso modello di Tecnigrafo: un responsabile con l'ultima parola
   su architettura, priorità e dipendenze; le regole le fanno rispettare gli
   strumenti.

## Alternative considerate

- **Mantenere l'ambizione originale** (riscrivere booleane, raccordi e tolleranze):
  rischio di anni di lavoro senza un componente utilizzabile e senza modo di
  sapere se il risultato supera OCCT.
- **Mettere il kernel dentro il repository di Tecnigrafo** (come dice la sua #217,
  che scartava un repository separato per non duplicare regole e governo). Si
  sceglie il repository separato perché i componenti hanno cicli di vita e
  dipendenze diversi (OCCT è pesante e serve solo al 3D), il kernel si prova e si
  misura da solo e il 2D di Tecnigrafo non deve ereditare OCCT. Il costo, regole
  e governo duplicati, si limita perché il responsabile è lo stesso e le regole
  sono le stesse.
- **Usare OCCT direttamente nel modulo `modeling` di Tecnigrafo**: nessuna
  possibilità di sostituirne un pezzo senza toccare il resto.

## Conseguenze

- Le ambizioni del primo manifesto restano documentate, non perse.
- L'avvio è più lento di un manifesto di ricerca, ma ogni tappa produce qualcosa di
  misurabile e di utile a Tecnigrafo.
- Le decisioni #104 e #217 di Tecnigrafo vanno aggiornate dal suo responsabile in
  modo coerente con questo ADR; qui non si modifica Tecnigrafo.
